<?php

/*foreach($_POST as $key => $value)
{echo "<p>" . $key . " is " . $value . "</p>";}*/
// $Id$

require "grab_globals.inc.php";
require "systemdefaults.inc.php";
require "areadefaults.inc.php";
require "config.inc.php";
require "internalconfig.inc.php";
require "wfunctions.inc";
require "wdb.inc";
require "standard_vars.inc.php";
require "trailer.inc";


function invalid_booking($message)
{
  global $day, $month, $year, $area, $room;

  print_header($day, $month, $year, $area, isset($room) ? $room : "");
  echo "<h1>" . get_vocab('invalid_booking') . "</h1>\n";
  echo "<p>$message</p>\n";
  // Print footer and exit
  print_footer(TRUE);
}

// Get non-standard form variables
$formvars = array(
    'user'              => 'string',
    'description'        => 'string',
    'start_seconds'      => 'int',
    'start_day'          => 'int',
    'start_month'        => 'int',
    'start_year'         => 'int',
    'end_seconds'        => 'int',
    'end_day'            => 'int',
    'end_month'          => 'int',
    'end_year'           => 'int',
    'all_day'            => 'string',  // bool, actually
    'type'               => 'string',
    'rooms'              => 'array',
    'original_room_id'   => 'int',
    'ical_uid'           => 'string',
    'ical_sequence'      => 'int',
    'ical_recur_id'      => 'string',
    'returl'             => 'string',
    'id'                 => 'int',
    'rep_id'             => 'int',
    'edit_type'          => 'string',
    'rep_type'           => 'int',
    'rep_end_day'        => 'int',
    'rep_end_month'      => 'int',
    'rep_end_year'       => 'int',
    'rep_id'             => 'int',
    'rep_day'            => 'array',   // array of bools
    'rep_num_weeks'      => 'int',
    'month_type'         => 'int',
    'month_absolute'     => 'int',
    'month_relative_ord' => 'string',
    'month_relative_day' => 'string',
    'skip'               => 'string',  // bool, actually
    'private'            => 'string',  // bool, actually
    'confirmed'          => 'string',
    'back_button'        => 'string',
    'timetohighlight'    => 'int',
    'page'               => 'string',
    'commit'             => 'string');

foreach($formvars as $var => $var_type)
{
  $$var = get_form_var($var, $var_type);
  // Trim the strings
  if (is_string($$var))
  {
    $$var = trim($$var);
  }
}

// BACK:  we didn't really want to be here - send them to the returl
if (!empty($back_button))
{
  /*  if (empty($returl))
    {
      $returl = "index.php";
    }*/
  header("Location: $returl");
  exit();
}

// Get custom form variables
$custom_fields = array();

// Get the information about the fields in the entry table
$fields = sql_field_info(times);

foreach($fields as $field)
{
  if (!in_array($field['name'], $standard_fields['entry']))
  {
    switch($field['nature'])
    {
      case 'character':
        $f_type = 'string';
        break;
      case 'integer':
        $f_type = 'int';
        break;
      // We can only really deal with the types above at the moment
      default:
        $f_type = 'string';
        break;
    }
    $var = VAR_PREFIX . $field['name'];
    if ($field['name']=='user')
    {$var = $field['name'];}
    $custom_fields[$field['name']] = get_form_var($var, $f_type);
    if (($f_type == 'int') && ($custom_fields[$field['name']] === ''))
    {
      $custom_fields[$field['name']] = NULL;
    }
    // Trim any strings
    if (is_string($custom_fields[$field['name']]))
    {
      $custom_fields[$field['name']] = trim($custom_fields[$field['name']]);
    }
  }
}



if (isset($month_relative_ord) && isset($month_relative_day))
{
  $month_relative = $month_relative_ord . $month_relative_day;
}

// When All Day is checked, $start_seconds and $end_seconds are disabled and so won't
// get passed through by the form.   We therefore need to set them.
if (!empty($all_day))
{
  if ($enable_periods)
  {
    $start_seconds = 12 * SECONDS_PER_HOUR;
    // This is actually the start of the last period, which is what the form would
    // have returned.   It will get corrected in a moment.
    $end_seconds = $start_seconds + ((count($periods) - 1) * 60);
  }
  else
  {
    $start_seconds = (($morningstarts * 60) + $morningstarts_minutes) * 60;
    $end_seconds = (($eveningends * 60) + $eveningends_minutes) *60;
    $end_seconds += $resolution;  // We want the end of the last slot, not the beginning
  }
}

// If we're operating on a booking day that stretches past midnight, it's more convenient
// for the sections past midnight to be shown as being on the day before.  That way the
// $returl will end up taking us back to the day we started on
if (day_past_midnight())
{
  $end_last = (((($eveningends * 60) + $eveningends_minutes) *60) + $resolution) % SECONDS_PER_DAY;
  if ($start_seconds < $end_last)
  {
    $start_seconds += SECONDS_PER_DAY;
    $day_before = getdate(mktime(0, 0, 0, $start_month, $start_day-1, $start_year));
    $start_day = $day_before['mday'];
    $start_month = $day_before['mon'];
    $start_year = $day_before['year'];
  }
}


// Now work out the start and times
$start_time = mktime(0, 0, $start_seconds, $start_month, $start_day, $start_year);
$end_time   = mktime(0, 0, $end_seconds, $end_month, $end_day, $end_year);



// Round down the start_time and round up the end_time to the nearest slot boundaries
// (This step is probably unnecesary now that MRBS always returns times aligned
// on slot boundaries, but is left in for good measure).
$am7 = get_start_first_slot($start_month, $start_day, $start_year);
$start_time = round_t_down($start_time, $resolution, $am7);
$end_time = round_t_up($end_time, $resolution, $am7);

// If they asked for 0 minutes, and even after the rounding the slot length is still
// 0 minutes, push that up to 1 resolution unit.
if ($end_time == $start_time)
{
  $end_time += $resolution;
}

if (isset($rep_type) && ($rep_type != REP_NONE) &&
    isset($rep_end_month) && isset($rep_end_day) && isset($rep_end_year))
{
  // Get the repeat entry settings
  $end_date = mktime(intval($start_seconds/SECONDS_PER_HOUR),
      intval(($start_seconds%SECONDS_PER_HOUR)/60),
      0,
      $rep_end_month, $rep_end_day, $rep_end_year);
}
else
{
  $rep_type = REP_NONE;
  $end_date = 0;  // to avoid an undefined variable notice
}

if (!isset($rep_day))
{
  $rep_day = array();
}

$rep_opt = "";


// Get the repeat details
if (isset($rep_type) && ($rep_type != REP_NONE))
{
  if ($rep_type == REP_WEEKLY)
  {
    // If no repeat day has been set, then set a default repeat day
    // as the day of the week of the start of the period
    if (count($rep_day) == 0)
    {
      $rep_day[] = date('w', $start_time);
    }
    // Build string of weekdays to repeat on:
    for ($i = 0; $i < 7; $i++)
    {
      $rep_opt .= in_array($i, $rep_day) ? "1" : "0";  // $rep_opt is a string
    }
  }

  // Make sure that the start_time coincides with a repeat day.  In
  // other words make sure that the first start_time defines an actual
  // entry.   We need to do this because if we are going to construct an iCalendar
  // object, RFC 5545 demands that the start time is the first event of
  // a series.  ["The "DTSTART" property for a "VEVENT" specifies the inclusive
  // start of the event.  For recurring events, it also specifies the very first
  // instance in the recurrence set."]

  $rep_details = array('rep_type'      => $rep_type,
      'rep_opt'       => $rep_opt,
      'rep_num_weeks' => $rep_num_weeks);
  if (isset($month_type))
  {
    if ($month_type == REP_MONTH_ABSOLUTE)
    {
      $rep_details['month_absolute'] = $month_absolute;
    }
    else
    {
      $rep_details['month_relative'] = $month_relative;
    }
  }

  // Get the first entry in the series and make that the start time
  $reps = mrbsGetRepeatEntryList($start_time, $end_date, $rep_details, 1);

  if (count($reps) > 0)
  {
    $duration = $end_time - $start_time;
    $duration -= cross_dst($start_time, $end_time);
    $start_time = $reps[0];
    $end_time = $start_time + $duration;
    $start_day = date('j', $start_time);
    $start_month = date('n', $start_time);
    $start_year = date('Y', $start_time);
  }
}


if (!$ajax || !$commit)
{
  // Get the start day/month/year and make them the current day/month/year
  $day = $start_day;
  $month = $start_month;
  $year = $start_year;
}


// Set up the return URL.    As the user has tried to book a particular room and a particular
// day, we must consider these to be the new "sticky room" and "sticky day", so modify the
// return URL accordingly.

// First get the return URL basename, having stripped off the old query string
//   (1) It's possible that $returl could be empty, for example if edit_entry.php had been called
//       direct, perhaps if the user has it set as a bookmark
//   (2) Avoid an endless loop.   It shouldn't happen, but just in case ...
//   (3) If you've come from search, you probably don't want to go back there (and if you did we'd
//       have to preserve the search parameter in the query string)
$returl_base   = explode('?', basename($returl));
if (empty($returl) || ($returl_base[0] == "edit_entry.php") || ($returl_base[0] == "edit_entry_handler.php")
    || ($returl_base[0] == "search.php"))
{
  switch ($default_view)
  {
    /*case "month":
      $returl = "month.php";
      break;
    case "week":
      $returl = "week.php";
      break;*/
    default:
      $returl = "test.php";
  }
}
else
{
  $returl = $returl_base[0];
}

// If we haven't been given a sensible date then get out of here and don't try and make a booking
if (!isset($start_day) || !isset($start_month) || !isset($start_year) || !checkdate($start_month, $start_day, $start_year))
{
  header("Location: $returl");
  exit;
}

// Now construct the new query string
$returl .= "?year=$year&month=$month&day=$day";

// Complete the query string
$returl .= "&user=$user";


// (4) Assemble the booking data
// -----------------------------

// Assemble an array of bookings, one for each room
$bookings = array();


$booking = array();
$booking['type'] = $type;
$booking['user'] = $user;
$booking['number'] = $number;
$booking['start_time'] = $start_time;
$booking['end_time'] = $end_time;
$booking['rep_type'] = $rep_type;
$booking['rep_opt'] = $rep_opt;
$booking['rep_num_weeks'] = $rep_num_weeks;
$booking['end_date'] = $end_date;
$booking['ical_uid'] = $ical_uid;
$booking['ical_sequence'] = $ical_sequence;
$booking['ical_recur_id'] = $ical_recur_id;
if ($booking['rep_type'] == REP_MONTHLY)
{
  if ($month_type == REP_MONTH_ABSOLUTE)
  {
    $booking['month_absolute'] = $month_absolute;
  }
  else
  {
    $booking['month_relative'] = $month_relative;
  }
}

// Do the custom fields
foreach ($custom_fields as $key => $value)
{
  $booking[$key] = $value;
}

// Set the various bits in the status field as appropriate
// (Note: the status field is the only one that can differ by room)
$status = 0;
// Privacy status
if ($isprivate)
{
  $status |= STATUS_PRIVATE;  // Set the private bit
}
// If we are using booking approvals then we need to work out whether the
// status of this booking is approved.   If the user is allowed to approve
// bookings for this room, then the status will be approved, since they are
// in effect immediately approving their own booking.  Otherwise the booking
// will need to approved.
if ($approval_enabled && !auth_book_admin($user, $room_id))
{
  $status |= STATUS_AWAITING_APPROVAL;
}
// Confirmation status
if ($confirmation_enabled && !$confirmed)
{
  $status |= STATUS_TENTATIVE;
}
$booking['status'] = $status;

$bookings[] = $booking;


$just_check = $ajax && function_exists('json_encode') && !$commit;
$this_id = (isset($id)) ? $id : NULL;
$result = mrbsMakeBookings($bookings, $this_id, $just_check, $skip, $original_room_id, $need_to_send_mail, $edit_type);

// If we weren't just checking and this was a succesful booking and
// we were editing an existing booking, then delete the old booking
if (!$just_check && $result['valid_booking'] && isset($id))
{
  echo "test";
  mrbsDelEntry($user, $id, ($edit_type == "series"), 1);
  //print_r(array_values($test));*/
  //echo sql_command("DELETE FROM $tbl_entry WHERE id=" . $id);
}

// If this is an Ajax request, output the result and finish
if ($ajax && function_exists('json_encode'))
{
  // If this was a successful commit generate the new HTML
  if ($result['valid_booking'] && $commit)
  {
    // Generate the new HTML
    require_once "functions_table.inc";
    if ($page == 'day')
    {
      $result['table_innerhtml'] = day_table_innerhtml($day, $month, $year, $room, $area, $timetohighlight);
    }
    else
    {
      $result['table_innerhtml'] = week_table_innerhtml($day, $month, $year, $room, $area, $timetohighlight);
    }
  }
  echo json_encode($result);
  exit;
}

// Everything was OK.   Go back to where we came from
if ($result['valid_booking'])
{
  header("Location: $returl");
  exit;
}

else
{
  print_header($day, $month, $year, $area, isset($room) ? $room : "");

  echo "<h2>" . get_vocab("sched_conflict") . "</h2>\n";
  if (!empty($result['rules_broken']))
  {
    echo "<p>\n";
    echo get_vocab("rules_broken") . ":\n";
    echo "</p>\n";
    echo "<ul>\n";
    foreach ($result['rules_broken'] as $rule)
    {
      echo "<li>$rule</li>\n";
    }
    echo "</ul>\n";
  }
  if (!empty($result['conflicts']))
  {
    echo "<p>\n";
    echo get_vocab("conflict").":\n";
    echo "</p>\n";
    echo "<ul>\n";
    foreach ($result['conflicts'] as $conflict)
    {
      echo "<li>$conflict</li>\n";
    }
    echo "</ul>\n";
  }
}

echo "<div id=\"submit_buttons\">\n";

// Back button
echo "<form method=\"post\" action=\"" . htmlspecialchars($returl) . "\">\n";
echo "<fieldset><legend></legend>\n";
echo "<input type=\"submit\" value=\"" . get_vocab("back") . "\">\n";
echo "</fieldset>\n";
echo "</form>\n";


// Skip and Book button (to book the entries that don't conflict)
// Only show this button if there were no policies broken and it's a series
if (empty($result['rules_broken'])  &&
    isset($rep_type) && ($rep_type != REP_NONE))
{
  echo "<form method=\"post\" action=\"" . htmlspecialchars(this_page()) . "\">\n";
  echo "<fieldset><legend></legend>\n";
  // Put the booking data in as hidden inputs
  $skip = 1;  // Force a skip next time round
  // First the ordinary fields
  foreach ($formvars as $var => $var_type)
  {
    if ($var_type == 'array')
    {
      // See the comment at the top of the page about array formats
      foreach ($$var as $value)
      {
        if (isset($value))
        {
          echo "<input type=\"hidden\" name=\"${var}[]\" value=\"" . htmlspecialchars($value) . "\">\n";
        }
      }
    }
    elseif (isset($$var))
    {
      echo "<input type=\"hidden\" name=\"$var\" value=\"" . htmlspecialchars($$var) . "\">\n";
    }
  }
  // Then the custom fields
  foreach($fields as $field)
  {
    if (array_key_exists($field['name'], $custom_fields) && isset($custom_fields[$field['name']]))
    {
      echo "<input type=\"hidden\"" .
          " name=\"" . VAR_PREFIX . $field['name'] . "\"" .
          " value=\"" . htmlspecialchars($custom_fields[$field['name']]) . "\">\n";
    }
  }
  // Submit button
  echo "<input type=\"submit\"" .
      " value=\"" . get_vocab("skip_and_book") . "\"" .
      " title=\"" . get_vocab("skip_and_book_note") . "\">\n";
  echo "</fieldset>\n";
  echo "</form>\n";
}

echo "</div>\n";

output_trailer();

