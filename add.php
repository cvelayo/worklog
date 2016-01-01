<?php

// $Id$

require "defaultincludes.inc";
require_once "mrbs_sql.inc";

// Get non-standard form variables
$name = $_POST['name'];
$team = $_POST['team'];
$code = $_POST['code'];
$role = $_POST['role'];
$description = get_form_var('description', 'string');
//$capacity = get_form_var('capacity', 'int');
$type = get_form_var('type', 'string');

/*// Check the user is authorised for this page
checkAuthorised();*/

// This file is for adding new areas/rooms

$error = '';
$table = ($type == 'room' ? 'codes' : 'users');
// First of all check that we've got an area or room name
if (!isset($code) || ($code === ''))
{
  $error = "empty_name";
}

elseif (sql_mysqli_query1("SELECT COUNT(*) FROM $table WHERE code = '$code'") > 0)
{$error = "duplicate";}

// we need to do different things depending on if its a room
// or an area
elseif ($type == "area")
{
  //$area = mrbsAddArea($name, $error);
  $sql = "INSERT INTO users (name, code, team, role, disabled)
          VALUES ('$name', '$code', '$team', '$role', 0)";
  if (!sql_mutex_lock("users"))
  {
    fatal_error(TRUE, get_vocab("failed_to_acquire"));
  }
  if (sql_command($sql) < 0)
  {
    trigger_error(sql_error(), E_USER_WARNING);
    fatal_error(TRUE, get_vocab("fatal_db_error"));
  }
  $area = sql_insert_id('users', 'id');
  sql_mutex_unlock("users");
}

elseif ($type == "room")
{
  //$room = mrbsAddRoom($name, $area, $error, $description, $capacity);
  $f2f = $_POST['f2f'];
  $available = $_POST['available'];
  $dnka = $_POST['dnka'];
  $outreach = $_POST['outreach'];
  $sql = "INSERT INTO codes (code, description, f2f, available, dnka, outreach, disabled)
          VALUES ('$code', '$description', $f2f, $available, $dnka, $outreach, 0)";
  if (!sql_mutex_lock("users"))
  {
    fatal_error(TRUE, get_vocab("failed_to_acquire"));
  }
  if (sql_command($sql) < 0)
  {
    trigger_error(sql_error(), E_USER_WARNING);
    fatal_error(TRUE, get_vocab("fatal_db_error"));
  }
  $area = sql_insert_id('users', 'id');
  sql_mutex_unlock("users");
}

$returl = "admin.php?success=".(isset($area) ? 1:0) . (!empty($error) ? "&error=$error" : "");
header("Location: $returl");

