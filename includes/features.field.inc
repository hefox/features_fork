<?php

/**
 * Implements hook_features_api().
 */
function field_features_api() {
  return array(
    'field_base' => array(
      'name' => t('Field Base'),
      'default_hook' => 'field_default_field_bases',
      'default_file' => FEATURES_DEFAULTS_INCLUDED,
      'feature_source' => TRUE,
    ),
    'field_instance' => array(
      'name' => t('Fields Instance'),
      'default_hook' => 'field_default_field_instances',
      'default_file' => FEATURES_DEFAULTS_INCLUDED,
      'feature_source' => TRUE,
    ),
    'field' => array(
      'name' => t('Fields'),
      'default_hook' => 'field_default_fields',
      'default_file' => FEATURES_DEFAULTS_INCLUDED,
      'feature_source' => TRUE,
    )
  );
}


/**
 * Implements hook_features_export_options().
 */
function field_base_features_export_options() {
  $options = array();
  foreach (field_info_fields() as $field_name => $info) {
    $options[$field_name] = $field_name;
  }
  return $options;
}

/**
 * Implements hook_features_export_options().
 */
function field_instance_features_export_options() {
  $options = array();
  $instances = field_info_instances();
  foreach ($instances as $entity_type => $bundles) {
    foreach ($bundles as $bundle => $fields) {
      foreach ($fields as $field) {
        $identifier = "{$entity_type}-{$bundle}-{$field['field_name']}";
        $options[$identifier] = $identifier;
      }
    }
  }
  return $options;
}

/**
 * Implements hook_features_export().
 */
function field_base_features_export($data, &$export, $module_name = '') {
  $pipe = array();
  $map = features_get_default_map('field_base');

  // The field_default_fields() hook integration is provided by the
  // features module so we need to add it as a dependency.
  $export['dependencies']['features'] = 'features';

  foreach ($data as $identifier) {
    if ($field = features_field_base_load($identifier)) {
      $export['features']['field_base'][$identifier] = $identifier;
      $export['dependencies'][$field['module']] = $field['module'];
      $export['dependencies'][$field['storage']['module']] = $field['storage']['module'];
      // If taxonomy field, add in the vocabulary
      if ($field['type'] == 'taxonomy_term_reference' && !empty($field['settings']['allowed_values'])) {
        foreach ($field['settings']['allowed_values'] as $allowed_values) {
          if (!empty($allowed_values['vocabulary'])) {
            $pipe['taxonomy'][] = $allowed_values['vocabulary'];
          }
        }
      }
    }
  }
  return $pipe;
}


/**
 * Implements hook_features_export().
 */
function field_instance_features_export($data, &$export, $module_name = '') {
  $pipe = array('field_base' => array());
  $map = features_get_default_map('field');

  // The field_default_fields() hook integration is provided by the
  // features module so we need to add it as a dependency.
  $export['dependencies']['features'] = 'features';

  foreach ($data as $identifier) {
    if ($field = features_field_instance_load($identifier)) {
      $export['features']['field_instance'][$identifier] = $identifier;
      $export['dependencies'][$field['field_instance']['widget']['module']] = $field['field_instance']['widget']['module'];
      foreach ($field['field_instance']['display'] as $key => $display) {
        if (isset($display['module'])) {
          $export['dependencies'][$display['module']] = $display['module'];
          // @TODO: handle the pipe to image styles
        }
      }
      $pipe['field_base'][$field['field_config']['field_name']] = $field['field_config']['field_name'];
    }
  }
  return $pipe;
}


/**
 * Implements hook_features_export_render().
 */
function field_base_features_export_render($module, $data, $export = NULL) {
  $translatables = $code = array();

  $code[] = '  $field_bases = array();';
  $code[] = '';
  foreach ($data as $identifier) {
    if ($field = features_field_base_load($identifier)) {
      unset($field['columns']);
      unset($field['locked']);
      // Only remove the 'storage' declaration if the field is using the default
      // storage type.
      if ($field['storage']['type'] == variable_get('field_storage_default', 'field_sql_storage')) {
        unset($field['storage']);
      }
      // If we still have a storage declaration here it means that a non-default
      // storage type was altered into to the field definition. And noone would
      // never need to change the 'details' key, so don't render it.
      if (isset($field['storage']['details'])) {
        unset($field['storage']['details']);
      }

      _field_features_export_sort($field);
      $field_export = features_var_export($field, '  ');
      $field_identifier = features_var_export($identifier);
      $code[] = "  // Exported field_base: {$field_identifier}";
      $code[] = "  \$field_bases[{$field_identifier}] = {$field_export};";
      $code[] = "";
    }
  }
  $code[] = '  return $field_bases;';
  $code = implode("\n", $code);
  return array('field_default_field_bases' => $code);
}


/**
 * Implements hook_features_export_render().
 */
function field_instance_features_export_render($module, $data, $export = NULL) {
  $translatables = $code = array();

  $code[] = '  $field_instances = array();';
  $code[] = '';
  foreach ($data as $identifier) {
    if ($field = features_field_instance_load($identifier)) {
      $instance = $field['field_instance'];
  
      _field_features_export_sort($instance);
      $field_export = features_var_export($instance, '  ');
      $field_identifier = features_var_export($identifier);
      $code[] = "  // Exported field_instance: {$field_identifier}";
      $code[] = "  \$field_instances[{$field_identifier}] = {$field_export};";
      $code[] = "";

      // Add label and description to translatables array.
      if (!empty($field['field_instance']['label'])) {
        $translatables[] = $field['field_instance']['label'];
      }
      if (!empty($field['field_instance']['description'])) {
        $translatables[] = $field['field_instance']['description'];
      }
    }
  }
  if (!empty($translatables)) {
    $code[] = features_translatables_export($translatables, '  ');
  }
  $code[] = '  return $field_instances;';
  $code = implode("\n", $code);
  return array('field_default_field_instances' => $code);
}

// Helper to enforce consistency in field export arrays.
function _field_features_export_sort(&$field, $sort = TRUE) {

  // Some arrays are not sorted to preserve order (for example allowed_values).
  static $sort_blacklist = array('allowed_values');

  if ($sort) {
    ksort($field);
  }
  foreach ($field as $k => $v) {
    if (is_array($v)) {
      _field_features_export_sort($field[$k], !in_array($k, $sort_blacklist));
    }
  }
}

/**
 * Implements hook_features_revert().
 */
function field_base_features_revert($module) {
  field_base_features_rebuild($module);
}

/**
 * Implements hook_features_revert().
 */
function field_instance_features_revert($module) {
  field_instance_features_rebuild($module);
}

/**
 * Implements of hook_features_rebuild().
 * Rebuilds fields from code defaults.
 */
function field_base_features_rebuild($module) {
  if ($fields = features_get_default('field_base', $module)) {
    field_info_cache_clear();

    foreach ($fields as $field) {
      // Create or update field.
      if ($existing_field = field_info_field($field['field_name'])) {
        field_update_field($field);
      }
      else {
        field_create_field($field);
      }
      variable_set('menu_rebuild_needed', TRUE);
    }
  }
}

/**
 * Implements of hook_features_rebuild().
 * Rebuilds fields from code defaults.
 */
function field_instance_features_rebuild($module) {
  if ($fields = features_get_default('field', $module)) {
    field_info_cache_clear();

    foreach ($fields as $field) {
      // If the field base information does not exist yet, cancel out.
      if (!field_info_field($field['field_config']['field_name'])) {
        continue;
      }
      // Create or update field instance.
      $field_instance = $field['field_instance'];
      $existing_instance = field_info_instance($field_instance['entity_type'], $field_instance['field_name'], $field_instance['bundle']);
      if ($existing_instance) {
        field_update_instance($field_instance);
      }
      else {
        field_create_instance($field_instance);
      }
      variable_set('menu_rebuild_needed', TRUE);
    }
  }
}

/**
 * Load a field's configuration and instance configuration by an
 * entity_type-bundle-field_name identifier.
 */
function features_field_instance_load($identifier) {
  list($entity_type, $bundle, $field_name) = explode('-', $identifier);
  $field_info = field_info_field($field_name);
  $instance_info = field_info_instance($entity_type, $field_name, $bundle);
  if ($field_info && $instance_info) {
    unset($field_info['id']);
    unset($field_info['bundles']);
    unset($instance_info['id']);
    unset($instance_info['field_id']);
    return array(
      'field_config' => $field_info,
      'field_instance' => $instance_info,
    );
  }
  return FALSE;
}

/**
 * Load a field's base information, removing uneeded information.
 */
function features_field_base_load($field_name) {
  if ($field_info = field_info_field($field_name)) {
    unset($field_info['id']);
    unset($field_info['bundles']);
    return $field_info;
  }
  return FALSE;
}

/**
 * Implements hook_features_export().
 *
 * Converts all fields to field instances.
 */
function field_features_export($data, &$export, $module_name = '') {
  return array('field_instance' => $data);
}

/**
 * Implements hook_features_revert().
 */
function field_features_revert($module) {
  field_features_rebuild($module);
}

/**
 * Implements of hook_features_rebuild().
 *
 * DEPRACTED: Ensure that newly enabled, non-updated features still work.
 */
function field_features_rebuild($module) {
  if ($fields = features_get_default('field', $module)) {
    field_info_cache_clear();

    foreach ($fields as $field) {
      // Create or update field.
      $field_config = $field['field_config'];
      if ($existing_field = field_info_field($field_config['field_name'])) {
        field_update_field($field_config);
      }
      else {
        field_create_field($field_config);
      }

      // Create or update field instance.
      $field_instance = $field['field_instance'];
      $existing_instance = field_info_instance($field_instance['entity_type'], $field_instance['field_name'], $field_instance['bundle']);
      if ($existing_instance) {
        field_update_instance($field_instance);
      }
      else {
        field_create_instance($field_instance);
      }
      variable_set('menu_rebuild_needed', TRUE);
    }
  }
}

