<?php

/**
 * @file
 * All of the menu, pages, and forms related to bundle administration.
 */

/**
 * This function creates the menu items relevant to bundle administration
 *
 * @param $entity_type
 *  (String) entity type
 *
 * This function called is triggered from hook_menu()
 * @see eck_menu()
 */
function eck__bundle__menu($entity_type) {
  $entity_type_label = eck__get_entity_type_label($entity_type);

  // Create the menus relavant to types.
  $menu = array();

  // DELETE ENTITY TYPE
  $menu["admin/structure/eck/{$entity_type}/delete"] = array(
    'title' => "Delete a(n) '{$entity_type_label}'",
    'description' => "Delete the '{$entity_type_label}' Entity Type",
    'page callback' => "drupal_get_form",//"eck__entity_type__delete",
    'page arguments' => array("eck__entity_type__delete_form", $entity_type),
    'access arguments' => array("delete {$entity_type} entity type"),
    'file' => 'eck.entity_type.inc'
  );

  // OVERVIEW Entity types.
  $menu["admin/structure/eck/{$entity_type}"] = array(
    'title' => "{$entity_type_label} Bundles" ,
    'description' => "View all the bundles for '{$entity_type_label}'",
    'page callback' => "eck__bundle__overview",
    'page arguments' => array($entity_type),
    'access arguments' => array("administer {$entity_type} bundles"),
    'weight' => 0,
    'file' => 'eck.bundle.inc'
  );

  $menu["admin/structure/eck/{$entity_type}/add"] = array(
    'title' => "Add a(n) '{$entity_type_label}' Bundle ",
    'description' => "Add a(n) new '{$entity_type_label} Bundle'",
    'page callback' => "drupal_get_form",
    'page arguments' => array('eck__bundle__add', $entity_type),
    'access arguments' => array("add {$entity_type} bundles"),
    'type' => MENU_LOCAL_ACTION,
    'weight' => 1,
    'file' => 'eck.bundle.inc'
  );

  module_load_include('inc', 'eck', 'eck.entity');
  foreach (eck__get_bundles($entity_type) as $bundle) {
    $menu = array_merge($menu, eck__entity__menu($entity_type, $bundle));
  }

  return $menu;
}

/**
 * Page call back for the bundle overview table (to see and manipulate all created label of
 * a given type)
 *
 * @param entity_type
 *  (String) entity type
 */
function eck__bundle__overview($entity_type) {
  $header = array(t('Type'), array('data' => t('Operations'), 'colspan' => '1'));
  $rows = array();

  foreach (eck__get_bundles($entity_type) as $bundle) {
    $bundle_label =  eck__get_bundle_label($entity_type, $bundle);

    $uri = "admin/structure/eck/{$entity_type}/{$bundle}";
    $rows[] = 
    array(
      l($bundle_label, url($uri, array('absolute' => TRUE))),
      l("delete", $uri."/delete")
    );
  }

  $build['bundle_table'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
  );

  $build['sample_code'] = array(
    '#theme' => 'form_element',
    '#title' => t('Optional entity class'),
    '#description' => t('You may add this custom class the entity. To create a new instance of this class programmatically call: %code', array(
      '%code' => "entity_create('{$entity_type}', array());"
    )),
    '#children' => "<pre>" . check_plain("class " . eck_get_class_name($entity_type, 'EntityType') . " extends Entity{\n\n}") . "</pre>"
  );

  return $build;
}

/**
 * ADD Entity types.
 *
 * @param $form
 *  Form array provided by the Form API
 * @param $form_state
 *  array provided by the Form API
 * @param entity_type
 *  (String) entity type
 */
function eck__bundle__add($form, &$form_state, $entity_type) {
  $form['entity_type'] = array(
    '#type' => 'value',
    '#value' => $entity_type,
  );

  $form['bundle_label'] = array(
    '#type' => 'textfield',
    '#title' => "Type",
    '#description' => "A Human readable name for the bundle",
  );

  $form['bundle'] = array(
    '#type' => 'machine_name',
    '#required' => FALSE,
    '#machine_name' => array(
      'exists' => '_eck_fake_exists',
      'source' => array('bundle_label'),
    )
  );

  $form['#validate'][] = 'eck__bundle__add_validate';

  $form['submit'] = array(
    '#type' => 'submit',
    '#weight' => 10000,
    '#value' => t('Save'),
  );

  return $form;
}

/**
 * Validation for bundle creation (Make sure this bundle don't exist for thie entity type)
 *
 * @param $form
 *  Form array provided by the Form API
 * @param $form_state
 *  array provided by the Form API
 */
function eck__bundle__add_validate($form, &$form_state) {
  $entity_type = $form_state['values']['entity_type'];

  // The type does not have to be unique in the table, but it should be unique
  // to its entity so we will check that here.
  foreach (eck__get_bundles($entity_type) as $bundle) {
    if ($bundle == $form_state['values']['bundle']) {
      form_set_error('bundle', t("bundle '{$bundle} already exists for this entity"));
    }
  }
}

/**
 * Submit function for add form
 *
 * @param $form
 *  Form array provided by the Form API
 * @param $form_state
 *  array provided by the Form API
 */
function eck__bundle__add_submit($form, &$form_state) {
  $entity_type = $form_state['values']['entity_type'];
  $bundle = $form_state['values']['bundle'];
  $bundle_label = $form_state['values']['bundle_label'];

  db_insert('eck_types')
      ->fields(array(
        'entity' => $entity_type,
        'type' => $bundle,
        'label' => $bundle_label,
      ))
      ->execute();

  drupal_set_message(t('the %bundle for entity type %entity_type has been saved', array('%bundle' => $bundle, '%entity_type' => $entity_type)));

  drupal_get_schema(NULL, TRUE);
  entity_info_cache_clear();
  menu_rebuild();

  drupal_goto("admin/structure/eck/{$entity_type}/{$bundle}");
}

/**
 * Delete the bundle of a given entity type
 * @param $entity_type
 *  (String) The entity type of the bundle that will be deleted
 * @param  $bundle
 *  (String) The bundle to be deleted
 */
function eck__bundle__delete($entity_type, $bundle){
  //first delete all of the entities of this bundle
  $query = new EntityFieldQuery();
  $query
  ->entityCondition('entity_type', $entity_type, '=')
  ->entityCondition('bundle', $bundle,'=');
  
  $results = $query->execute();
  
  if(!empty($results)){
    $ids = array_keys($results[$entity_type]);
    entity_delete($entity_type, $ids);
  }
  //then we delete the bundle (field_instances)
  field_attach_delete_bundle($entity_type, $bundle);
  
  //and finally we delete the bundle from the eck_type table
  db_delete('eck_types')
  ->condition('entity', $entity_type)
  ->condition('type', $bundle)
  ->execute();
  
  menu_rebuild();
  drupal_flush_all_caches();
  
  drupal_set_message("The bundle '{$bundle}' from the entity type '$entity_type' has been deleted");
  //return "<h1>Deletion Completed</h1>
  //<h3>The bundle '{$bundle}' from the entity type '$entity_type' has been deleted<h3>";
}

function eck__bundle__delete_form($form, &$form_state, $entity_type, $bundle) {
  $form['entity_type'] =
      array(
        '#type' => 'value',
        '#value' => $entity_type,
  );

  $form['bundle'] =
      array(
        '#type' => 'value',
        '#value' => $bundle,
  );

  $form['submit_redirect'] =
  array(
    '#type' => 'value',
    '#value' => "admin/structure/eck/{$entity_type}",
  );

  $message = t("Are you sure that you want to delete the bundle '{$bundle}'");

  $caption = t("All of the data (entities) associated with this bundle
  will be deleted. This action cannot be undone.");

  return confirm_form($form, $message, "admin/structure/eck/{$entity_type}", $caption, t('Delete'));
}

/**
 * Sumbmit function for the delete functionality
 *
 * @param $form
 *  Form array provided by the Form API
 * @param $form_state
 *  array provided by the Form API
 */
function eck__bundle__delete_form_submit($form, &$form_state) {
  $entity_type = $form_state['values']['entity_type'];
  $bundle = $form_state['values']['bundle'];
  $submit_redirect = $form_state['values']['submit_redirect'];

  eck__bundle__delete($entity_type, $bundle);

  // Ok, lets delete the entity
  $form_state['redirect'] = $submit_redirect;
}