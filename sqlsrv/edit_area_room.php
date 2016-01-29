<?php
// $Id$

// If you want to add some extra columns to the room table to describe the room
// then you can do so and this page should automatically recognise them and handle
// them.    At the moment support is limited to the following column types:
//
// MySQL        PostgreSQL            Form input type
// -----        ----------            ---------------
// bigint       bigint                text
// int          integer               text
// mediumint                          text
// smallint     smallint              checkbox
// tinyint                            checkbox
// text         text                  textarea
// tinytext                           textarea
//              character varying     textarea
// varchar(n)   character varying(n)  text/textarea, depending on the value of n
//              character             text
// char(n)      character(n)          text/textarea, depending on the value of n
//
// NOTE 1: For char(n) and varchar(n) fields, a text input will be presented if
// n is less than or equal to $text_input_max, otherwise a textarea box will be
// presented.
//
// NOTE 2: PostgreSQL booleans are not supported, due to difficulties in
// handling the fields in a database independent way (a PostgreSQL boolean
// will return a PHP boolean type when read by a PHP query, whereas a MySQL
// tinyint returns an int).   In order to have a boolean field in the room
// table you should use a smallint in PostgreSQL or a smallint or a tinyint
// in MySQL.
//
// You can put a description of the column that will be used as the label in
// the form in the $vocab_override variable in the config file using the tag
// 'room.[columnname]'.
//
// For example if you want to add a column specifying whether or not a room
// has a coffee machine you could add a column to the room table called
// 'coffee_machine' of type tinyint(1), in MySQL, or smallint in PostgreSQL.
// Then in the config file you would add the line
//
// $vocab_override['en']['room.coffee_machine'] = "Coffee machine";  // or appropriate translation
//
// If MRBS can't find an entry for the field in the lang file or vocab overrides, then
// it will use the fieldname, eg 'coffee_machine'.

/*require "defaultincludes.inc";
require_once "mrbs_sql.inc";*/
require "connection.php";
require "grab_globals.inc.php";
require "systemdefaults.inc.php";
require "areadefaults.inc.php";
require "config.inc.php";
require "internalconfig.inc.php";
require "wfunctions.inc";
require "wdb.inc";
require "standard_vars.inc.php";
require_once "mincals.inc";
require "trailer.inc";

function create_field_entry_timezone()
{
  global $timezone, $zoneinfo_outlook_compatible;
  
  $special_group = "Others";
  
  echo "<div>\n";
  echo "<label for=\"area_timezone\">" . get_vocab("timezone") . ":</label>\n";

  // If possible we'll present a list of timezones that this server supports and
  // which also have a corresponding VTIMEZONE definition.
  // Otherwise we'll just have to let the user type in a timezone, which introduces
  // the possibility of an invalid timezone.
  if (function_exists('timezone_identifiers_list'))
  {
    $timezones = array();
    $timezone_identifiers = timezone_identifiers_list();
    foreach ($timezone_identifiers as $value)
    {
      if (strpos($value, '/') === FALSE)
      {
        // There are some timezone identifiers (eg 'UTC') on some operating
        // systems that don't fit the Continent/City model.   We'll put them
        // into the special group
        $continent = $special_group;
        $city = $value;
      }
      else
      {
        // Note: timezone identifiers can have three components, eg
        // America/Argentina/Tucuman.    To keep things simple we will
        // treat anything after the first '/' as a single city and
        // limit the explosion to two
        list($continent, $city) = explode('/', $value, 2);
      }
      // Check that there's a VTIMEZONE definition
      $tz_dir = ($zoneinfo_outlook_compatible) ? TZDIR_OUTLOOK : TZDIR;  
      $tz_file = "$tz_dir/$value.ics";
      // UTC is a special case because we can always produce UTC times in iCalendar
      if (($city=='UTC') || file_exists($tz_file))
      {
        $timezones[$continent][] = $city;
      }
    }
    
    echo "<select id=\"area_timezone\" name=\"area_timezone\">\n";
    foreach ($timezones as $continent => $cities)
    {
      if (count($cities) > 0)
      {
        echo "<optgroup label=\"" . htmlspecialchars($continent) . "\">\n";
        foreach ($cities as $city)
        {
          if ($continent == $special_group)
          {
            $timezone_identifier = $city;
          }
          else
          {
            $timezone_identifier = "$continent/$city";
          }
          echo "<option value=\"" . htmlspecialchars($timezone_identifier) . "\"" .
               (($timezone_identifier == $timezone) ? " selected=\"selected\"" : "") .
               ">" . htmlspecialchars($city) . "</option>\n";
        }
        echo "</optgroup>\n";
      }
    }
    echo "</select>\n";
  }
  // There is no timezone_identifiers_list() function so we'll just let the
  // user type in a timezone
  else
  {
    echo "<input id=\"area_timezone\" name=\"area_timezone\" value=\"" . htmlspecialchars($timezone) . "\">\n";
  }
  
  echo "</div>\n";
}


function create_field_entry_advance_booking()
{
  global $min_create_ahead_secs, $max_create_ahead_secs,
         $min_delete_ahead_secs, $max_delete_ahead_secs,
         $min_create_ahead_enabled, $max_create_ahead_enabled,
         $min_delete_ahead_enabled, $max_delete_ahead_enabled,
         $enable_periods;
         
  $min_create_ahead_value = $min_create_ahead_secs;
  toTimeString($min_create_ahead_value, $min_create_ahead_units);
  $max_create_ahead_value = $max_create_ahead_secs;
  toTimeString($max_create_ahead_value, $max_create_ahead_units);
  
  $min_delete_ahead_value = $min_delete_ahead_secs;
  toTimeString($min_delete_ahead_value, $min_delete_ahead_units);
  $max_delete_ahead_value = $max_delete_ahead_secs;
  toTimeString($max_delete_ahead_value, $max_delete_ahead_units);
  
  // Note when using periods
  echo "<div id=\"book_ahead_periods_note\"" .
       (($enable_periods) ? '' : ' class="js_none"') .
       ">\n";
  echo "<label></label><span>" . get_vocab("book_ahead_note_periods") . "</span>";
  echo "</div>\n";
  
  $units = array("seconds", "minutes", "hours", "days", "weeks");
  $options = array();
  foreach ($units as $unit)
  {
    $options[$unit] = get_vocab($unit);
  }
  
  echo "<fieldset>\n";
  echo "<legend>" . get_vocab("booking_creation") . "</legend>\n";
  // Minimum book ahead
  echo "<div>\n";
  $params = array('label' => get_vocab("min_book_ahead") . ":",
                  'name'  => 'area_min_create_ahead_enabled',
                  'value' => $min_create_ahead_enabled,
                  'class' => 'enabler');
  generate_checkbox($params);
  $attributes = array('class="text"',
                      'type="number"',
                      'step="1"');
  $params = array('name'       => 'area_min_create_ahead_value',
                  'value'      => $min_create_ahead_value,
                  'attributes' => $attributes);
  generate_input($params);
  $params = array('name'    => 'area_min_create_ahead_units',
                  'value'   => array_search($min_create_ahead_units, $options),
                  'options' => $options);
  generate_select($params);
  echo "</div>\n";
  
  
  // Maximum book ahead
  echo "<div>\n";
  $params = array('label' => get_vocab("max_book_ahead") . ":",
                  'name'  => 'area_max_create_ahead_enabled',
                  'value' => $max_create_ahead_enabled,
                  'class' => 'enabler');
  generate_checkbox($params);
  $attributes = array('class="text"',
                      'type="number"',
                      'step="1"');
  $params = array('name'       => 'area_max_create_ahead_value',
                  'value'      => $max_create_ahead_value,
                  'attributes' => $attributes);
  generate_input($params);
  $params = array('name'    => 'area_max_create_ahead_units',
                  'value'   => array_search($max_create_ahead_units, $options),
                  'options' => $options);  // options same as before
  generate_select($params);
  echo "</div>\n";
  echo "</fieldset>\n";
  
  
  
  echo "<fieldset>\n";
  echo "<legend>" . get_vocab("booking_deletion") . "</legend>\n";
  // Minimum book ahead
  echo "<div>\n";
  $params = array('label' => get_vocab("min_book_ahead") . ":",
                  'name'  => 'area_min_delete_ahead_enabled',
                  'value' => $min_delete_ahead_enabled,
                  'class' => 'enabler');
  generate_checkbox($params);
  $attributes = array('class="text"',
                      'type="number"',
                      'step="1"');
  $params = array('name'       => 'area_min_delete_ahead_value',
                  'value'      => $min_delete_ahead_value,
                  'attributes' => $attributes);
  generate_input($params);
  $params = array('name'    => 'area_min_delete_ahead_units',
                  'value'   => array_search($min_delete_ahead_units, $options),
                  'options' => $options);
  generate_select($params);
  echo "</div>\n";
  
  // Maximum book ahead
  echo "<div>\n";
  $params = array('label' => get_vocab("max_book_ahead") . ":",
                  'name'  => 'area_max_delete_ahead_enabled',
                  'value' => $max_delete_ahead_enabled,
                  'class' => 'enabler');
  generate_checkbox($params);
  $attributes = array('class="text"',
                      'type="number"',
                      'step="1"');
  $params = array('name'       => 'area_max_delete_ahead_value',
                  'value'      => $max_delete_ahead_value,
                  'attributes' => $attributes);
  generate_input($params);
  $params = array('name'    => 'area_max_delete_ahead_units',
                  'value'   => array_search($max_delete_ahead_units, $options),
                  'options' => $options);  // options same as before
  generate_select($params);
  echo "</div>\n";
  echo "</fieldset>\n";
}


function create_field_entry_max_number()
{
  global $interval_types,
         $max_per_interval_area_enabled, $max_per_interval_global_enabled,
         $max_per_interval_area, $max_per_interval_global;
         
  // The max_per booking policies
  echo "<fieldset>\n";
  echo "<legend>" . get_vocab("booking_limits") . "</legend>\n";
  echo "<table>\n";
      
  echo "<thead>\n";
  echo "<tr>\n";
  echo "<th></th>\n";
  echo "<th>" . get_vocab("this_area") . "</th>\n";
  echo "<th title=\"" . get_vocab("whole_system_note") . "\">" . get_vocab("whole_system") . "</th>\n";
  echo "</tr>\n";
  echo "</thead>\n";
      
  echo "<tbody>\n";
  foreach ($interval_types as $interval_type)
  {
    echo "<tr>\n";
    echo "<td><label>" . get_vocab("max_per_${interval_type}") . ":</label></td>\n";
    echo "<td><input class=\"enabler checkbox\" type=\"checkbox\" id=\"area_max_per_${interval_type}_enabled\" name=\"area_max_per_${interval_type}_enabled\"" .
         (($max_per_interval_area_enabled[$interval_type]) ? " checked=\"checked\"" : "") .
         ">\n";
    echo "<input class=\"text\" type=\"number\" min=\"0\" step=\"1\" name=\"area_max_per_${interval_type}\" value=\"$max_per_interval_area[$interval_type]\"></td>\n"; 
    echo "<td>\n";
    echo "<input class=\"checkbox\" type=\"checkbox\" disabled=\"disabled\"" .
         (($max_per_interval_global_enabled[$interval_type]) ? " checked=\"checked\"" : "") .
         ">\n";
    echo "<input class=\"text\" disabled=\"disabled\" value=\"" . $max_per_interval_global[$interval_type] . "\">\n";
    echo "</td>\n";
    echo "</tr>\n";
  }
  echo "</tbody>\n";
      
  echo "</table>\n";
  echo "</fieldset>\n";
}


function create_field_entry_max_duration()
{
  global $max_duration_enabled, $max_duration_secs, $max_duration_periods;
  
  // The max duration policies
  echo "<fieldset>\n";
  echo "<legend>" . get_vocab("booking_durations") . "</legend>\n";

  echo "<div>\n";
  $params = array('label' => get_vocab("max_duration") . ":",
                  'name'  => 'area_max_duration_enabled',
                  'value' => $max_duration_enabled,
                  'class' => 'enabler');
  generate_checkbox($params);
  echo "</div>\n";
  
  echo "<div>\n";
  $attributes = array('class="text"',
                      'type="number"',
                      'min="0"',
                      'step="1"');
  $params = array('name'       => 'area_max_duration_periods',
                  'label'      => get_vocab("mode_periods") . ':',
                  'value'      => $max_duration_periods,
                  'attributes' => $attributes);
  generate_input($params);
  echo "</div>\n";
  
  echo "<div>\n";
  $max_duration_value = $max_duration_secs;
  toTimeString($max_duration_value, $max_duration_units);
  $attributes = array('class="text"',
                      'type="number"',
                      'min="0"',
                      'step="1"');
  $params = array('name'       => 'area_max_duration_value',
                  'label'      => get_vocab("mode_times") . ':',
                  'value'      => $max_duration_value,
                  'attributes' => $attributes);
  generate_input($params);
  
  $units = array("seconds", "minutes", "hours", "days", "weeks");
  $options = array();
  foreach ($units as $unit)
  {
    $options[$unit] = get_vocab($unit);
  }
  $params = array('name'    => 'area_max_duration_units',
                  'value'   => array_search($max_duration_units, $options),
                  'options' => $options);
  generate_select($params);
  echo "</div>\n";
  
  echo "</fieldset>\n";
}


// Get non-standard form variables
$user = get_form_var('user', 'int');
$phase = get_form_var('phase', 'int');
$new_area = get_form_var('new_area', 'int');
$old_area = get_form_var('old_area', 'int');
$room_name = get_form_var('room_name', 'string');
$room_disabled = get_form_var('room_disabled', 'string');
$sort_key = get_form_var('sort_key', 'string');
$old_room_name = get_form_var('old_room_name', 'string');
$area_name = get_form_var('area_name', 'string');
$description = get_form_var('description', 'string');
$capacity = get_form_var('capacity', 'int');
$room_admin_email = get_form_var('room_admin_email', 'string');
$area_disabled = get_form_var('area_disabled', 'string');
$area_timezone = get_form_var('area_timezone', 'string');
$area_admin_email = get_form_var('area_admin_email', 'string');
$area_morningstarts = get_form_var('area_morningstarts', 'int');
$area_morningstarts_minutes = get_form_var('area_morningstarts_minutes', 'int');
$area_morning_ampm = get_form_var('area_morning_ampm', 'string');
$area_res_mins = get_form_var('area_res_mins', 'int');
$area_def_duration_mins = get_form_var('area_def_duration_mins', 'int');
$area_def_duration_all_day = get_form_var('area_def_duration_all_day', 'string');
$area_eveningends = get_form_var('area_eveningends', 'int');
$area_eveningends_minutes = get_form_var('area_eveningends_minutes', 'int');
$area_evening_ampm = get_form_var('area_evening_ampm', 'string');
$area_eveningends_t = get_form_var('area_eveningends_t', 'int');
$area_min_create_ahead_enabled = get_form_var('area_min_create_ahead_enabled', 'string');
$area_min_create_ahead_value = get_form_var('area_min_create_ahead_value', 'int');
$area_min_create_ahead_units = get_form_var('area_min_create_ahead_units', 'string');
$area_max_create_ahead_enabled = get_form_var('area_max_create_ahead_enabled', 'string');
$area_max_create_ahead_value = get_form_var('area_max_create_ahead_value', 'int');
$area_max_create_ahead_units = get_form_var('area_max_create_ahead_units', 'string');
$area_min_delete_ahead_enabled = get_form_var('area_min_delete_ahead_enabled', 'string');
$area_min_delete_ahead_value = get_form_var('area_min_delete_ahead_value', 'int');
$area_min_delete_ahead_units = get_form_var('area_min_delete_ahead_units', 'string');
$area_max_delete_ahead_enabled = get_form_var('area_max_delete_ahead_enabled', 'string');
$area_max_delete_ahead_value = get_form_var('area_max_delete_ahead_value', 'int');
$area_max_delete_ahead_units = get_form_var('area_max_delete_ahead_units', 'string');
$area_max_duration_enabled = get_form_var('area_max_duration_enabled', 'string');
$area_max_duration_periods = get_form_var('area_max_duration_periods', 'int');
$area_max_duration_value = get_form_var('area_max_duration_value', 'int');
$area_max_duration_units = get_form_var('area_max_duration_units', 'string');
$area_private_enabled = get_form_var('area_private_enabled', 'string');
$area_private_default = get_form_var('area_private_default', 'int');
$area_private_mandatory = get_form_var('area_private_mandatory', 'string');
$area_private_override = get_form_var('area_private_override', 'string');
$area_approval_enabled = get_form_var('area_approval_enabled', 'string');
$area_reminders_enabled = get_form_var('area_reminders_enabled', 'string');
$area_enable_periods = get_form_var('area_enable_periods', 'string');
$area_confirmation_enabled = get_form_var('area_confirmation_enabled', 'string');
$area_confirmed_default = get_form_var('area_confirmed_default', 'string');
$custom_html = get_form_var('custom_html', 'string');  // Used for both area and room, but you only ever have one or the other
$change_done = get_form_var('change_done', 'string');
$change_room = get_form_var('change_room', 'string');
$change_area = get_form_var('change_area', 'string');
$tbl_room = 'users';
// Get the max_per_interval form variables
foreach ($interval_types as $interval_type)
{
  $var = "area_max_per_${interval_type}";
  $$var = get_form_var($var, 'int');
  $var = "area_max_per_${interval_type}_enabled";
  $$var = get_form_var($var, 'string');
}

// Get the information about the fields in the room table
$fields = sql_field_info($tbl_room);
$user = $_GET['user'];
// Get any user defined form variables
foreach($fields as $field)
{
  switch($field['nature'])
  {
    case 'character':
      $type = 'string';
      break;
    case 'integer':
      $type = 'int';
      break;
    // We can only really deal with the types above at the moment
    default:
      $type = 'string';
      break;
  }
  $var = VAR_PREFIX . $field['name'];
  $$var = get_form_var($var, $type);
  if (($type == 'int') && ($$var === ''))
  {
    unset($$var);
  }
}

/*// Check the user is authorised for this page
checkAuthorised();

// Also need to know whether they have admin rights
$user = getUserName();
$required_level = (isset($max_level) ? $max_level : 2);
$is_admin = (authGetUserLevel($user) >= $required_level);*/

// Done changing area or room information?
if (isset($change_done))
{
/*  if (!empty($room)) // Get the area the room is in
  {
    $area = mrbsGetRoomArea($room);
  }*/
  Header("Location: admin.php?day=$day&month=$month&year=$year&area=$area");
  exit();
}

// Intialise the validation booleans
$valid_email = TRUE;
$valid_resolution = TRUE;
$enough_slots = TRUE;
$valid_area = TRUE;
$valid_room_name = TRUE;



// PHASE 2
// -------
if ($phase == 2)
{
  // Unauthorised users shouldn't normally be able to reach Phase 2, but just in case
  // they have, check again that they are allowed to be here
  $is_admin = 1;
  if (isset($change_room) || isset($change_area))
  {
    if (!$is_admin)
    {
      showAccessDenied($day, $month, $year, $area, "");
      exit();
    }
  }
  
  require_once "functions_mail.inc";

  // PHASE 2 (ROOM) - UPDATE THE DATABASE
  // ------------------------------------
  $description = $_POST['description'];
$f2f = $_POST['f2f'];
$code = $_POST['code'];
$available = $_POST['available'];
$outreach = $_POST['outreach'];
$dnka = $_POST['dnka'];
$disabled = $_POST['disabled'];
$id = $_POST['id'];
  if (isset($change_room) && !empty($code))
  {
/*    // clean up the address list replacing newlines by commas and removing duplicates
    $room_admin_email = clean_address_list($room_admin_email);
    // put a space after each comma so that the list displays better
    $room_admin_email = str_replace(',', ', ', $room_admin_email);
    // validate the email addresses
    $valid_email = validate_email_list($room_admin_email);*/

    if (FALSE != $valid_email)
    {
      if (empty($capacity))
      {
        $capacity = 0;
      }
/*
      // Acquire a mutex to lock out others who might be deleting the new area
      if (!sql_mutex_lock("codes"))
      {
        fatal_error(TRUE, get_vocab("failed_to_acquire"));
      }
      // Check the new area still exists
      if (sql_query1("SELECT COUNT(*) FROM $tbl_area WHERE id=$new_area LIMIT 1") < 1)
      {
        $valid_area = FALSE;
      }
      // If so, check that the room name is not already used in the area
      // (only do this if you're changing the room name or the area - if you're
      // just editing the other details for an existing room we don't want to reject
      // the edit because the room already exists!)
      // [SQL escaping done by sql_syntax_casesensitive_equals()]
      elseif ( (($new_area != $old_area) || ($room_name != $old_room_name))
              && sql_query1("SELECT COUNT(*)
                               FROM $tbl_room
                              WHERE" . sql_syntax_casesensitive_equals("room_name", $room_name) . "
                                AND area_id=$new_area
                              LIMIT 1") > 0)
      {
        $valid_room_name = FALSE;
      }*/
      // If everything is still OK, update the databasae

        // Convert booleans into 0/1 (necessary for PostgreSQL)
/*        $room_disabled = (!empty($room_disabled)) ? 1 : 0;
        $sql = "UPDATE $tbl_room SET ";*/
/*        $n_fields = count($fields);
        $assign_array = array();
        foreach ($fields as $field)
        {
          if ($field['name'] != 'id')  // don't do anything with the id field
          {
            switch ($field['name'])
            {
              // first of all deal with the standard MRBS fields
              case 'area_id':
                $assign_array[] = "area_id=$new_area";
                break;
              case 'disabled':
                $assign_array[] = "disabled=$room_disabled";
                break;
              case 'room_name':
                $assign_array[] = "room_name='" . sql_escape($room_name) . "'";
                break;
              case 'sort_key':
                $assign_array[] = "sort_key='" . sql_escape($sort_key) . "'";
                break;
              case 'description':
                $assign_array[] = "description='" . sql_escape($description) . "'";
                break;
              case 'capacity':
                $assign_array[] = "capacity=$capacity";
                break;
              case 'room_admin_email':
                $assign_array[] = "room_admin_email='" . sql_escape($room_admin_email) . "'";
                break;
              case 'custom_html':
                $assign_array[] = "custom_html='" . sql_escape($custom_html) . "'";
                break;
              // then look at any user defined fields
              default:
                $var = VAR_PREFIX . $field['name'];
                switch ($field['nature'])
                {
                  case 'integer':
                    if (!isset($$var) || ($$var === ''))
                    {
                      // Try and set it to NULL when we can because there will be cases when we
                      // want to distinguish between NULL and 0 - especially when the field
                      // is a genuine integer.
                      $$var = ($field['is_nullable']) ? 'NULL' : 0;
                    }
                    break;
                  default:
                    $$var = "'" . sql_escape($$var) . "'";
                    break;
                }
                $assign_array[] = sql_quote($field['name']) . "=" . $$var;
                break;
            }
          }
        }

        $sql .= implode(",", $assign_array) . " WHERE id=$room";*/
        $sql = "UPDATE codes
                SET code = '$code', description = '$description', f2f= '$f2f', disabled = '$disabled', outreach='$outreach', available = '$available', dnka = '$dnka'
                WHERE id = $id";

        if (sql_command($sql) < 0)
        {
          echo get_vocab("update_room_failed") . "<br>\n";
          trigger_error(sql_error(), E_USER_WARNING);
          fatal_error(FALSE, get_vocab("fatal_db_error"));
        }
        // if everything is OK, release the mutex and go back to
        // the admin page (for the new area)
        //sql_mutex_unlock("codes");

        Header("Location: admin.php?day=$day&month=$month&year=$year&success=1");
        exit();

    
      // Release the mutex
      //sql_mutex_unlock("codes");
    }
  }

  // PHASE 2 (AREA) - UPDATE THE DATABASE
  // ------------------------------------
$user = $_POST['user'];
$name = $_POST['name'];
$code = $_POST['code'];
$team = $_POST['team'];
$role = $_POST['role'];
$disabled = $_POST['disabled'];
$id = $_POST['id'];

  if (isset($change_area) && !empty($user))
  { 
   /* // clean up the address list replacing newlines by commas and removing duplicates
    $area_admin_email = clean_address_list($area_admin_email);
    // put a space after each comma so that the list displays better
    $area_admin_email = str_replace(',', ', ', $area_admin_email);
    // validate email addresses
    $valid_email = validate_email_list($area_admin_email);
  */
    // Tidy up the input from the form
    /*if (isset($area_eveningends_t))
    {
      // if we've been given a time in minutes rather than hours and minutes, convert it
      // (this will happen if JavaScript is enabled)
      $area_eveningends_minutes = $area_eveningends_t % 60;
      $area_eveningends = ($area_eveningends_t - $area_eveningends_minutes)/60;
    }

    if (!empty($area_morning_ampm))
    {
      if (($area_morning_ampm == "pm") && ($area_morningstarts < 12))
      {
        $area_morningstarts += 12;
      }
      if (($area_morning_ampm == "am") && ($area_morningstarts > 11))
      {
        $area_morningstarts -= 12;
      }
    }

    if (!empty($area_evening_ampm))
    {
      if (($area_evening_ampm == "pm") && ($area_eveningends < 12))
      {
        $area_eveningends += 12;
      }
      if (($area_evening_ampm == "am") && ($area_eveningends > 11))
      {
        $area_eveningends -= 12;
      }
    }
  
    // Convert the book ahead times into seconds
    fromTimeString($area_min_create_ahead_value, $area_min_create_ahead_units);
    fromTimeString($area_max_create_ahead_value, $area_max_create_ahead_units);
    fromTimeString($area_min_delete_ahead_value, $area_min_delete_ahead_units);
    fromTimeString($area_max_delete_ahead_value, $area_max_delete_ahead_units);
    
    fromTimeString($area_max_duration_value, $area_max_duration_units);
    
    // If we are using periods, round these down to the nearest whole day
    // (anything less than a day is meaningless when using periods)
    if ($area_enable_periods)
    {
      $vars = array('area_min_create_ahead_value',
                    'area_max_create_ahead_value',
                    'area_min_delete_ahead_value',
                    'area_max_delete_ahead_value');
                    
      foreach ($vars as $var)
      {
        if (isset($$var))
        {
          $$var -= $$var % SECONDS_PER_DAY;
        }
      }
    }
  */
/*    // Convert booleans into 0/1 (necessary for PostgreSQL)
    $vars = array('area_disabled',
                  'area_def_duration_all_day',
                  'area_min_create_ahead_enabled',
                  'area_max_create_ahead_enabled',
                  'area_min_delete_ahead_enabled',
                  'area_max_delete_ahead_enabled',
                  'area_max_duration_enabled',
                  'area_private_enabled',
                  'area_private_default',
                  'area_private_mandatory',
                  'area_approval_enabled',
                  'area_reminders_enabled',
                  'area_enable_periods',
                  'area_confirmation_enabled',
                  'area_confirmed_default');
    foreach ($interval_types as $interval_type)
    {
      $vars[] = "area_max_per_${interval_type}_enabled";
    }
    foreach ($vars as $var)
    {
      $$var = (!empty($$var)) ? 1 : 0;
    }

    
    if (!$area_enable_periods)
    { 
      // Avoid divide by zero errors
      if ($area_res_mins == 0)
      {
        $valid_resolution = FALSE;
      }
      else
      {
        // Check morningstarts, eveningends, and resolution for consistency
        $start_first_slot = ($area_morningstarts*60) + $area_morningstarts_minutes;   // minutes
        $start_last_slot  = ($area_eveningends*60) + $area_eveningends_minutes;       // minutes
        
        // If eveningends is before morningstarts then it's really on the next day
        if (hm_before(array('hours' => $area_eveningends, 'minutes' => $area_eveningends_minutes),
                      array('hours' => $area_morningstarts, 'minutes' => $area_morningstarts_minutes)))
        {
          $start_last_slot += MINUTES_PER_DAY;
        }
        
        $start_difference = ($start_last_slot - $start_first_slot);         // minutes
        
        if ($start_difference%$area_res_mins != 0)
        {
          $valid_resolution = FALSE;
        }
      
        // Check that the number of slots we now have is no greater than $max_slots
        // defined in the config file - otherwise we won't generate enough CSS classes
        $n_slots = ($start_difference/$area_res_mins) + 1;
        if ($n_slots > $max_slots)
        {
          $enough_slots = FALSE;
        }
      }
    }*/
    
    // If everything is OK, update the database
    if ((FALSE != $valid_email) && (FALSE != $valid_resolution) && (FALSE != $enough_slots))
    {
      $sql = "UPDATE users SET name = '$name', code = '$code', team = '$team', role = '$role', disabled = '$disabled' WHERE id = $user";
      /*

      $assign_array[] = "reminders_enabled=" . $area_reminders_enabled;
      $assign_array[] = "enable_periods=" . $area_enable_periods;
      $assign_array[] = "confirmation_enabled=" . $area_confirmation_enabled$assign_array = array();
      $assign_array[] = "area_name='" . sql_escape($area_name) . "'";
      $assign_array[] = "disabled=" . $area_disabled;
      $assign_array[] = "timezone='" . sql_escape($area_timezone) . "'";
      $assign_array[] = "area_admin_email='" . sql_escape($area_admin_email) . "'";
      $assign_array[] = "custom_html='" . sql_escape($custom_html) . "'";
      if (!$area_enable_periods)
      {
        $assign_array[] = "resolution=" . $area_res_mins * 60;
        $assign_array[] = "default_duration=" . $area_def_duration_mins * 60;
        $assign_array[] = "default_duration_all_day=" . $area_def_duration_all_day;
        $assign_array[] = "morningstarts=" . $area_morningstarts;
        $assign_array[] = "morningstarts_minutes=" . $area_morningstarts_minutes;
        $assign_array[] = "eveningends=" . $area_eveningends;
        $assign_array[] = "eveningends_minutes=" . $area_eveningends_minutes;
      }

      // only update the min and max *_ahead_secs fields if the form values
      // are set;  they might be NULL because they've been disabled by JavaScript
      $assign_array[] = "min_create_ahead_enabled=" . $area_min_create_ahead_enabled;
      $assign_array[] = "max_create_ahead_enabled=" . $area_max_create_ahead_enabled;
      $assign_array[] = "min_delete_ahead_enabled=" . $area_min_delete_ahead_enabled;
      $assign_array[] = "max_delete_ahead_enabled=" . $area_max_delete_ahead_enabled;
      $assign_array[] = "max_duration_enabled=" . $area_max_duration_enabled;

      if (isset($area_min_create_ahead_value))
      {
        $assign_array[] = "min_create_ahead_secs=" . $area_min_create_ahead_value;
      }
      if (isset($area_max_create_ahead_value))
      {
        $assign_array[] = "max_create_ahead_secs=" . $area_max_create_ahead_value;
      }
      if (isset($area_min_delete_ahead_value))
      {
        $assign_array[] = "min_delete_ahead_secs=" . $area_min_delete_ahead_value;
      }
      if (isset($area_max_delete_ahead_value))
      {
        $assign_array[] = "max_delete_ahead_secs=" . $area_max_delete_ahead_value;
      }
      if (isset($area_max_duration_value))
      {
        $assign_array[] = "max_duration_secs=" . $area_max_duration_value;
        $assign_array[] = "max_duration_periods=" . $area_max_duration_periods;
      }

      foreach($interval_types as $interval_type)
      {
        $var = "max_per_${interval_type}_enabled";
        $area_var = "area_" . $var;
        $assign_array[] = "$var=" . $$area_var;

        $var = "max_per_${interval_type}";
        $area_var = "area_" . $var;
        if (isset($$area_var))
        {
          // only update these fields if they are set;  they might be NULL because
          // they have been disabled by JavaScript
          $assign_array[] = "$var=" . $$area_var;
        }
      }

      $assign_array[] = "private_enabled=" . $area_private_enabled;
      $assign_array[] = "private_default=" . $area_private_default;
      $assign_array[] = "private_mandatory=" . $area_private_mandatory;
      $assign_array[] = "private_override='" . $area_private_override . "'";
      $assign_array[] = "approval_enabled=" . $area_approval_enabled;;
      $assign_array[] = "confirmed_default=" . $area_confirmed_default;
            */
      //$sql .= implode(",", $assign_array) . " WHERE id=$area";

      if (sql_command($sql) < 0)
      {

        echo sql_error();
        echo get_vocab("update_area_failed") . "<br>\n";
        trigger_error(sql_error(), E_USER_WARNING);
        fatal_error(FALSE, get_vocab("fatal_db_error"));
      }
      // If the database update worked OK, go back to the admin page
      Header("Location: admin.php?day=$day&month=$month&year=$year&success=1");
      exit();
    }
  }
}

// PHASE 1 - GET THE USER INPUT
// ----------------------------

print_header($day, $month, $year, $user);
$is_admin =1;
if ($is_admin)
{
  // Heading is confusing for non-admins
  echo "<h2>" . get_vocab("editroomarea") . "</h2>\n";
}

// Non-admins will only be allowed to view room details, not change them
$disabled = !$is_admin;
$code = $_GET['code'];

// THE ROOM FORM
if (isset($change_room) && !empty($code))
{
  $res = sql_query("SELECT TOP 1 * FROM codes WHERE code='$code'");
  if (! $res)
  {
    fatal_error(0, get_vocab("error_room") . $room . get_vocab("not_found"));
  }

  $row = sql_mysqli_row_keyed($res, 0);

  echo "<h2>\n";
  echo ($is_admin) ? get_vocab("editroom") : get_vocab("viewroom");
  echo "</h2>\n";
  ?>
  <form class="form_general" id="edit_room" action="edit_area_room.php" method="post">
    <fieldset class="admin">
    <legend></legend>
  
      <fieldset>
      <legend></legend>
        <span class="error">
           <?php 
           // It's impossible to have more than one of these error messages, so no need to worry
           // about paragraphs or line breaks.
           echo ((FALSE == $valid_email) ? get_vocab('invalid_email') : "");
           echo ((FALSE == $valid_area) ? get_vocab('invalid_area') : "");
           echo ((FALSE == $valid_room_name) ? get_vocab('invalid_room_name') : "");
           ?>
        </span>
      </fieldset>
    
      <fieldset>
      <legend></legend>
      <input type="hidden" name="room" value="<?php echo $row["id"]?>">
    
      <?php
      //$areas = get_areas($all=TRUE);

      
      /*// The area select box
      echo "<div>\n";
      $params = array('label'         => get_vocab("area") . ":",
                      'name'          => 'new_area',
                      'options'       => $areas,
                      'force_assoc'   => TRUE,
                      'value'         => $row['area_id'],
                      'disabled'      => $disabled,
                      'create_hidden' => FALSE);
      generate_select($params);
      echo "<input type=\"hidden\" name=\"old_area\" value=\"" . $row['area_id'] . "\">\n";
      echo "</div>\n";*/
      
      // First of all deal with the standard MRBS fields
      // Room name
      echo "<div>\n";
      $params = array('label'         => get_vocab("code") . ":",
                      'name'          => 'code',
                      'value'         => $row['code'],
                      'disabled'      => $disabled,
                      'create_hidden' => FALSE);
      generate_input($params);
      echo "<input type=\"hidden\" name=\"old_room_name\" value=\"" . htmlspecialchars($row["room_name"]) . "\">\n";
      echo "</div>\n";

      // Description
      echo "<div>\n";
      $params = array('label'         => get_vocab("description") . ":",
                      'name'          => 'description',
                      'value'         => $row['description'],
                      'disabled'      => $disabled,
                      'create_hidden' => FALSE);
      generate_input($params);
      echo "</div>\n";

      // Status (Enabled or Disabled)
      if ($is_admin)
      {
        echo "<div>\n";
        $options = array('0' => get_vocab("enabled"),
                         '1' => get_vocab("disabled"));
        $params = array('label'         => get_vocab("status") . ":",
                        'label_title'   => get_vocab("disabled_room_note"),
                        'name'          => 'disabled',
                        'value'         => ($row['disabled']) ? '1' : '0',
                        'options'       => $options,
                        'force_assoc'   => TRUE,
                        'disabled'      => $disabled,
                        'create_hidden' => FALSE);
        generate_radio_group($params);
        echo "</div>\n";
      }
      //Face to face time
      echo "<div id=\"status\">\n";
      $options = array('0' => get_vocab("no"),
                   '1' => get_vocab("yes"));
      $params = array('label'       => get_vocab("f2f") . ":",
                  'label_title' => get_vocab("Does this count as face to face time?"),
                  'name'        => 'f2f',
                  'value'       => ($row['f2f']) ? '1' : '0',
                  'options'     => $options,
                  'force_assoc' => TRUE);
      generate_radio_group($params);
      echo "</div>\n";

      //Available time
      echo "<div id=\"status\">\n";
      $options = array('0' => get_vocab("no"),
                   '1' => get_vocab("yes"));
      $params = array('label'       => get_vocab("available") . ":",
                  'label_title' => get_vocab("Does this count as time made available?"),
                  'name'        => 'available',
                  'value'       => ($row['available']) ? '1' : '0',
                  'options'     => $options,
                  'force_assoc' => TRUE);
      generate_radio_group($params);
      echo "</div>\n";

      //DNKA codes
      echo "<div id=\"status\">\n";
      $options = array('0' => get_vocab("no"),
                   '1' => get_vocab("yes"));
      $params = array('label'       => get_vocab("dnka") . ":",
                  'label_title' => get_vocab("Does this count as a DNKA code?"),
                  'name'        => 'dnka',
                  'value'       => ($row['dnka']) ? '1' : '0',
                  'options'     => $options,
                  'force_assoc' => TRUE);
      generate_radio_group($params);
      echo "</div>\n";

      //Outreach time
      echo "<div id=\"status\">\n";
      $options = array('0' => get_vocab("no"),
                   '1' => get_vocab("yes"));
      $params = array('label'       => get_vocab("outreach") . ":",
                  'label_title' => get_vocab("Does this count as outreach time?"),
                  'name'        => 'outreach',
                  'value'       => ($row['outreach']) ? '1' : '0',
                  'options'     => $options,
                  'force_assoc' => TRUE);
      generate_radio_group($params);
      echo "</div>\n";

/*      // Sort key
      if ($is_admin)
      {
        echo "<div>\n";
        $params = array('label'         => get_vocab("sort_key") . ":",
                        'label_title'   => get_vocab("sort_key_note"),
                        'name'          => 'sort_key',
                        'value'         => $row['sort_key'],
                        'disabled'      => $disabled,
                        'create_hidden' => FALSE);
        generate_input($params);
        echo "</div>\n";
      }*/
      
      /*// Capacity
      echo "<div>\n";
      $params = array('label'         => get_vocab("capacity") . ":",
                      'name'          => 'capacity',
                      'value'         => $row['capacity'],
                      'disabled'      => $disabled,
                      'create_hidden' => FALSE);
      generate_input($params);
      echo "</div>\n";

      // Room admin email
      echo "<div>\n";
      $params = array('label'         => get_vocab("room_admin_email") . ":",
                      'label_title'   => get_vocab("email_list_note"),
                      'name'          => 'room_admin_email',
                      'value'         => $row['room_admin_email'],
                      'attributes'    => array('rows="4"', 'cols="40"'),
                      'disabled'      => $disabled,
                      'create_hidden' => FALSE);
      generate_textarea($params);
      echo "</div>\n";*/
      
      /*// Custom HTML
      if ($is_admin)
      {
        // Only show the raw HTML to admins.  Non-admins will see the rendered HTML
        echo "<div>\n";
        $params = array('label'         => get_vocab("custom_html") . ":",
                        'label_title'   => get_vocab("custom_html_note"),
                        'name'          => 'custom_html',
                        'value'         => $row['custom_html'],
                        'attributes'    => array('rows="4"', 'cols="40"'),
                        'disabled'      => $disabled,
                        'create_hidden' => FALSE);
        generate_textarea($params);
        echo "</div>\n";
      }*/
    
      // then look at any user defined fields  
      /*foreach ($fields as $field)
      {
        if (!in_array($field['name'], $standard_fields['room']))
        {
          echo "<div>\n";
          $params = array('label'         => get_loc_field_name('codes', $field['name']) . ":",
                          'name'          => $field['name'],
                          'value'         => $row[$field['name']],
                          'disabled'      => $disabled,
                          'create_hidden' => FALSE);
          // Output a checkbox if it's a boolean or integer <= 2 bytes (which we will
          // assume are intended to be booleans)
          echo $params['name'];
          if (($field['nature'] == 'boolean') ||
              (($field['nature'] == 'integer') && isset($field['length']) && ($field['length'] <= 2)) )
          {
            generate_checkbox($params);
          }
          // Output a textarea if it's a character string longer than the limit for a
          // text input
          elseif (($field['nature'] == 'character') && isset($field['length']) && ($field['length'] > $text_input_max))
          {
            $params['attributes'] = array('rows="4"', 'cols="40"');
            generate_textarea($params);
          }
          // Otherwise output a text input
          else
          {
            generate_input($params);
          }
          echo "</div>\n";
        }
      }*/
      echo "</fieldset>\n";
    
      // Submit and Back buttons (Submit only if they're an admin)  
      echo "<fieldset class=\"submit_buttons\">\n";
      echo "<legend></legend>\n";
      echo "<div id=\"edit_area_room_submit_back\">\n";
      echo "<input class=\"submit\" type=\"submit\" name=\"change_done\" value=\"" . get_vocab("backadmin") . "\">\n";
      echo "</div>\n";
      if ($is_admin)
      { 
        $id = $row['id'];
        echo "<div id=\"edit_area_room_submit_save\">\n";
        echo "<input type=\"hidden\" name=\"phase\" value=\"2\">";
        echo "<input type=\"hidden\" name=\"id\" value=\"$id\">";
        echo "<input class=\"submit default_action\" type=\"submit\" name=\"change_room\" value=\"" . get_vocab("change") . "\">\n";
        echo "</div>\n";
      }
      echo "</fieldset>\n";
        
      ?>
    </fieldset>
  </form>

  <?php
  // Now the custom HTML
  echo "<div id=\"custom_html\">\n";
  // no htmlspecialchars() because we want the HTML!
  echo (!empty($row['custom_html'])) ? $row['custom_html'] . "\n" : "";
  echo "</div>\n";
}

// THE AREA FORM
if (isset($change_area) &&!empty($user))
{
  // Only admins can see this form
  if (!$is_admin)
  {
    showAccessDenied($day, $month, $year, $area, "");
    exit();
  }
  // Get the details for this area
  $res = sql_query("SELECT TOP 1 * FROM [users] WHERE id=$user");

  if (! $res)
  {

    fatal_error(0, get_vocab("error_area") . $area . get_vocab("not_found"));
  }
  $row = sql_mysqli_row_keyed($res, 0);
  sqlsrv_free_stmt($res);
  // Get the settings for this area, from the database if they are there, otherwise from
  // the config file.    A little bit inefficient repeating the SQL query
  // we've just done, but it makes the code simpler and this page is not used very often.
  //get_area_settings($area);

  echo "<form class=\"form_general\" id=\"edit_area\" action=\"edit_area_room.php\" method=\"post\">\n";
  echo "<fieldset class=\"admin\">\n";
  echo "<legend>" . get_vocab("editarea") . "</legend>\n";
  
  // Any error messages
  echo "<fieldset>\n";
  echo "<legend></legend>\n";
  if (FALSE == $valid_email)
  {
    echo "<p class=\"error\">" .get_vocab('invalid_email') . "</p>\n";
  }
  if (FALSE == $valid_resolution)
  {
    echo "<p class=\"error\">" .get_vocab('invalid_resolution') . "</p>\n";
  }
  if (FALSE == $enough_slots)
  {
    echo "<p class=\"error\">" .get_vocab('too_many_slots') . "</p>\n";
  }
  echo "</fieldset>\n";
  
  echo "<fieldset>\n";
  //echo "<legend>" . get_vocab("general_settings") . "</legend>\n";
  echo "<input type=\"hidden\" name=\"area\" value=\"" . $row["id"] . "\">\n";
  
  // Area name  
  echo "<div>\n";
  $params = array('label' => get_vocab("username") . ":",
                  'name'  => 'name',
                  'value' => $row['name']);
  generate_input($params);
  echo "</div>\n";

  // Code
  echo "<div>\n";
  $params = array('label' => get_vocab("usercode") . ":",
                  'name'  => 'code',
                  'value' => $row['code']);
  generate_input($params);
  echo "</div>\n";

  // Team
  echo "<div>\n";
  $params = array('label' => get_vocab("team") . ":",
                  'name'  => 'team',
                  'value' => $row['team']);
  generate_input($params);
  echo "</div>\n";

  // Role
  echo "<div>\n";
  $params = array('label' => get_vocab("name") . ":",
                  'name'  => 'role',
                  'value' => $row['role']);
  generate_input($params);
  echo "</div>\n";

  // Status - Enabled or Disabled
  echo "<div id=\"status\">\n";
  $options = array('0' => get_vocab("enabled"),
                   '1' => get_vocab("disabled"));
  $params = array('label'       => get_vocab("status") . ":",
                  'label_title' => get_vocab("disabled_area_note"),
                  'name'        => 'disabled',
                  'value'       => ($row['disabled']) ? '1' : '0',
                  'options'     => $options,
                  'force_assoc' => TRUE);
  generate_radio_group($params);
  echo "</div>\n";
        
  /*// Timezone
  create_field_entry_timezone();
  */
  /*// Area admin email
  echo "<div>\n";
  $params = array('label'       => get_vocab("area_admin_email") . ":",
                  'label_title' => get_vocab("email_list_note"),
                  'name'        => 'area_admin_email',
                  'value'       => $row['area_admin_email'],
                  'attributes'  => array('rows="4"', 'cols="40"'));
  generate_textarea($params);
  echo "</div>\n";*/
      
  /*// The custom HTML
  echo "<div>\n";
  $params = array('label'       => get_vocab("custom_html") . ":",
                  'label_title' => get_vocab("custom_html_note"),
                  'name'        => 'custom_html',
                  'value'       => $row['custom_html'],
                  'attributes'  => array('rows="4"', 'cols="40"'));
  generate_textarea($params);
  echo "</div>\n";*/
        
  /*// Mode - Times or Periods
  echo "<div id=\"mode\">\n";
  $options = array('1' => get_vocab("mode_periods"),
                   '0' => get_vocab("mode_times"));
  $params = array('label'       => get_vocab("mode") . ":",
                  'name'        => 'area_enable_periods',
                  'value'       => ($enable_periods) ? '1' : '0',
                  'options'     => $options,
                  'force_assoc' => TRUE);
  generate_radio_group($params);
  echo "</div>\n";*/
      
  /*echo "</fieldset>\n";

  // If we're using JavaScript, don't display the time settings section
  // if we're using periods (the JavaScript will display it if we change)
  echo "<fieldset id=\"time_settings\"" .
       (($enable_periods) ? ' class="js_none"' : '') .
       ">\n";
  echo "<legend>" . get_vocab("time_settings");
  echo "<span class=\"js_none\">&nbsp;&nbsp;(" . get_vocab("times_only") . ")</span>";
  echo "</legend>\n";
  
  echo "<div class=\"div_time\">\n";
  echo "<label>" . get_vocab("area_first_slot_start") . ":</label>\n";
  if ($twentyfourhour_format)
  {
    $value = sprintf("%02d", $morningstarts);
  }
  elseif ($morningstarts > 12)
  {
    $value = $morningstarts - 12;
  } 
  elseif ($morningstarts == 0)
  {
    $value = 12;
  }
  else
  {
    $value = $morningstarts;
  } 
  $params = array('name'       => 'area_morningstarts',
                  'value'      => $value,
                  'attributes' => array('class="time_hour"', 'maxlength="2"'));
  generate_input($params);
  
  echo "<span>:</span>\n";
  
  $params = array('name'       => 'area_morningstarts_minutes',
                  'value'      => sprintf("%02d", $morningstarts_minutes),
                  'attributes' => array('class="time_minute"', 'maxlength="2"'));
  generate_input($params);
        
  if (!$twentyfourhour_format)
  {
    echo "<div class=\"group ampm\">\n";
    $checked = ($morningstarts < 12) ? "checked=\"checked\"" : "";
    echo "<label><input name=\"area_morning_ampm\" type=\"radio\" value=\"am\" $checked>" .
         utf8_strftime($strftime_format['ampm'], mktime(1,0,0,1,1,2000)) .
         "</label>\n";
    $checked = ($morningstarts >= 12) ? "checked=\"checked\"" : "";
    echo "<label><input name=\"area_morning_ampm\" type=\"radio\" value=\"pm\" $checked>" .
         utf8_strftime($strftime_format['ampm'], mktime(13,0,0,1,1,2000)) .
         "</label>\n";
    echo "</div>\n";
  }

  echo "</div>\n";
      
  echo "<div class=\"div_dur_mins\">\n";
  $params = array('label'      => get_vocab("area_res_mins") . ":",
                  'name'       => 'area_res_mins',
                  'value'      => $resolution/60,
                  'attributes' => 'type="number" min="1" step="1"');
  generate_input($params);
  echo "</div>\n";
      
  echo "<div class=\"div_dur_mins\">\n";
  $params = array('label'      => get_vocab("area_def_duration_mins") . ":",
                  'name'       => 'area_def_duration_mins',
                  'value'      => $default_duration/60,
                  'attributes' => 'type="number" min="1" step="1"');
  generate_input($params);

  $params = array('label'       => get_vocab("all_day"),
                  'label_after' => TRUE,
                  'name'        => 'area_def_duration_all_day',
                  'value'       => $default_duration_all_day);
  generate_checkbox($params);
  echo "</div>\n";
  
  echo "<div id=\"last_slot\" class=\"js_hidden\">\n";*/
  // The contents of this div will be overwritten by JavaScript if enabled.    The JavaScript version is a drop-down
  // select input with options limited to those times for the last slot start that are valid.   The options are
  // dynamically regenerated if the start of the first slot or the resolution change.    The code below is
  // therefore an alternative for non-JavaScript browsers.
/*  echo "<div class=\"div_time\">\n";
  if ($twentyfourhour_format)
  {
    $value = sprintf("%02d", $eveningends);
  }
  elseif ($eveningends > 12)
  {
    $value = $eveningends - 12;
  } 
  elseif ($eveningends == 0)
  {
    $value = 12;
  }
  else
  {
    $value = $eveningends;
  } 
  $params = array('label' => get_vocab("area_last_slot_start") . ":",
                  'name'  => 'area_eveningends',
                  'value' => $value,
                  'attributes' => array('class="time_hour"', 'maxlength="2"'));
  generate_input($params);

  echo "<span>:</span>\n";
  
  $params = array('name'       => 'area_eveningends_minutes',
                  'value'      => sprintf("%02d", $eveningends_minutes),
                  'attributes' => array('class="time_minute"', 'maxlength="2"'));
  generate_input($params);

  if (!$twentyfourhour_format)
  {
    echo "<div class=\"group ampm\">\n";
    $checked = ($eveningends < 12) ? "checked=\"checked\"" : "";
    echo "<label><input name=\"area_evening_ampm\" type=\"radio\" value=\"am\" $checked>" . 
         utf8_strftime($strftime_format['ampm'], mktime(1,0,0,1,1,2000)) . 
         "</label>\n";
    $checked = ($eveningends >= 12) ? "checked=\"checked\"" : "";
    echo "<label><input name=\"area_evening_ampm\" type=\"radio\" value=\"pm\" $checked>" .
         utf8_strftime($strftime_format['ampm'], mktime(13,0,0,1,1,2000)) .
         "</label>\n";
    echo "</div>\n";
  }
  echo "</div>\n";  
  echo "</div>\n";  // last_slot

  echo "</fieldset>\n";
  
  
  // Booking policies
  echo "<fieldset id=\"booking_policies\">\n";
  echo "<legend>" . get_vocab("booking_policies") . "</legend>\n";
  create_field_entry_advance_booking();
  create_field_entry_max_number();
  create_field_entry_max_duration();
  echo "</fieldset>\n";
  
  
  // Confirmation settings
  echo "<fieldset>\n";
  echo "<legend>" . get_vocab("confirmation_settings") . "</legend>\n";
  
  // Confirmation enabled
  echo "<div>\n";
  $params = array('label' => get_vocab("allow_confirmation") . ":",
                  'name'  => 'area_confirmation_enabled',
                  'value' => $confirmation_enabled);
  generate_checkbox($params);
  echo "</div>\n";
  
  $options = array('1' => get_vocab("default_confirmed"),
                   '0' => get_vocab("default_tentative"));
  $params = array('label'       => get_vocab("default_settings_conf") . ":",
                  'name'        => 'area_confirmed_default',
                  'options'     => $options,
                  'force_assoc' => TRUE,
                  'value'       => ($confirmed_default) ? '1' : '0');
  generate_radio_group($params);

  echo "</fieldset>\n";
      
  echo "<fieldset>\n";
  echo "<legend>" . get_vocab("approval_settings") . "</legend>\n";
  echo "<div>\n";
  $params = array('label' => get_vocab("enable_approval") . ":",
                  'name'  => 'area_approval_enabled',
                  'value' => $approval_enabled);
  generate_checkbox($params);
  echo "</div>\n";

  echo "<div>\n";
  $params = array('label' => get_vocab("enable_reminders") . ":",
                  'name'  => 'area_reminders_enabled',
                  'value' => $reminders_enabled);
  generate_checkbox($params);
  echo "</div>\n";
  echo "</fieldset>\n";
  
  
  echo "<fieldset>\n";
  echo "<legend>" . get_vocab("private_settings") . "</legend>\n";
  
  // Private enabled
  echo "<div>\n";
  $params = array('label' => get_vocab("allow_private") . ":",
                  'name'  => 'area_private_enabled',
                  'value' => $private_enabled);
  generate_checkbox($params);
  echo "</div>\n";

  // Private mandatory
  echo "<div>\n";
  $params = array('label' => get_vocab("force_private") . ":",
                  'name'  => 'area_private_mandatory',
                  'value' => $private_mandatory);
  generate_checkbox($params);
  echo "</div>\n";

  // Default privacy settings
  $options = array('1' => get_vocab("default_private"),
                   '0' => get_vocab("default_public"));
  $params = array('label'       => get_vocab("default_settings"),
                  'name'        => 'area_private_default',
                  'options'     => $options,
                  'force_assoc' => TRUE,
                  'value'       => ($private_default) ? '1' : '0');
  generate_radio_group($params);

  echo "</fieldset>\n";
    
  echo "<fieldset>\n";
  echo "<legend>" . get_vocab("private_display") . "</legend>\n";
  echo "<label>" . get_vocab("private_display_label");
  echo "<span id=\"private_display_caution\">";
  echo get_vocab("private_display_caution");
  echo "</span>";
  echo "</label>\n";

  echo "<div class=\"group\" id=\"private_override\">\n";
  $options = array('none'    => get_vocab("treat_respect"),
                   'private' => get_vocab("treat_private"),
                   'public'  => get_vocab("treat_public"));
  foreach ($options as $value => $text)
  {
    echo "<div>\n";
    $params = array('name'    => 'area_private_override',
                    'options' => array($value => $text),
                    'value'   => $private_override);
    generate_radio($params);
    echo "</div>\n";
  }
  echo "</div>\n";
  
      */?><!--
      </fieldset>-->
    
      <fieldset class="submit_buttons">
      <legend></legend>
        <div id="edit_area_room_submit_back">
          <input class="submit" type="submit" name="change_done" value="<?php echo get_vocab("backadmin") ?>">
        </div>
        <div id="edit_area_room_submit_save">
          <input type="hidden" name="phase" value="2">
          <input type="hidden" name="user" value="<?php echo $user?>">
          <input class="submit default_action" type="submit" name="change_area" value="<?php echo get_vocab("change") ?>">
        </div>
      </fieldset>
    
    </fieldset>
  </form>
  <?php
}

output_trailer();

