<?php
/**
 * Created by PhpStorm.
 * User: tanni
 * Date: 1/8/2016
 * Time: 12:40 PM
 */
require "connection.php";
define('INTERVAL_DAY', 0);

// Return the number of rows in a result object
function sql_count($r)
{
    return sqlsrv_num_rows($r);
}

// Run a SQL query, returns a result object or FALSE on failure
function sql_query($sql)
{
    global $conn;
    $params = array();
    $options = array("Scrollable"=>SQLSRV_CURSOR_KEYSET);
    return sqlsrv_query($conn, $sql, $params, $options);
}

// Return one row from a result object, as a numbered array
function sql_row($r, $i)
{
    global $conn;

    if ($i >= sqlsrv_num_rows($r))
    {
        sqlsrv_free_stmt($r);
        return 0;
    }
    sqlsrv_fetch($r);
    return sqlsrv_get_field($r,0);
}

// Execute an SQL query which should return a single non-negative number value.
// This is a lightweight alternative to sql_query, good for use with count(*)
// and similar queries. It returns -1 on error or if the query did not return
// exactly one value, so error checking is somewhat limited.
// It also returns -1 if the query returns a single NULL value, such as from
// a MIN or MAX aggregate function applied over no rows.
function sql_mysqli_query1 ($sql, $db_conn = null)
{
    global $conn;

    $params = array();
    $options = array("Scrollable"=>SQLSRV_CURSOR_KEYSET);
    $r = sqlsrv_query($conn, $sql, $params, $options);
    if (! $r)
    {
        return -1;
    }
    if ((sqlsrv_num_rows($r) != 1) || (sqlsrv_num_fields($r) != 1) ||
        (($row = sqlsrv_fetch($r)) == NULL))
    {
        $result = -1;
    }
    else
    {
        $result = sqlsrv_get_field($r, 0);
    }
    sqlsrv_free_stmt($r);
    return $result;
}

// Return a row from a result as an associative array keyed by field name.
// The first row is 0.
// This is actually upward compatible with sql_row since the underlying
// routing also stores the data under number indexes.
// When called with i >= number of rows in the result, cleans up from
// the query and returns 0.
function sql_mysqli_row_keyed ($r, $i, $db_conn = null)
{
    //sql_mysqli_ensure_handle($db_conn);
    global $conn;
    if ($i >= sqlsrv_num_rows($r))
    {
        sqlsrv_free_stmt($r);
        return 0;
    }
    sqlsrv_fetch($r,NULL,$i);

    // Use _assoc rather than _array because _array doesn't have
    // an ASSOC parameter.  No impact on PHP version support.
    return sqlsrv_fetch_array($r);
}

function sql_field_info($table)
{
    // Map MySQL types on to a set of generic types
    $nature_map = array('bigint' => 'integer',
        'nchar' => 'character',
        'double' => 'real',
        'float' => 'real',
        'int' => 'integer',
        'mediumint' => 'integer',
        'smallint' => 'integer',
        'text' => 'character',
        'tinyint' => 'integer',
        'tinytext' => 'character',
        'nvarchar' => 'character');

    // Length in bytes of MySQL integer types
    $int_bytes = array('bigint' => 8, // bytes
        'int' => 4,
        'mediumint' => 3,
        'smallint' => 2,
        'tinyint' => 1);
    $sql = "SELECT COLUMN_NAME, IS_NULLABLE, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH
                      FROM worklog.INFORMATION_SCHEMA.COLUMNS
                      WHERE TABLE_NAME = '$table'";
    $res = sql_query($sql);

    if( !$res )
    {
        echo "Error.\n";
        die( print_r( sqlsrv_errors(), true));
    }
    if ($res === FALSE)
    {
        echo "Error.\n";
        die( print_r( sqlsrv_errors(), true));
    }
    else
    {
        $fields = array();
        for ($i = 0; ($row = sql_mysqli_row_keyed($res, $i)); $i++) {
            $name = $row['COLUMN_NAME'];
            $type = $row['DATA_TYPE'];
            // split the type (eg 'varchar(25)') around the opening '('
            $parts = explode('(', $type);
            // map the type onto one of the generic natures, if a mapping exists
            $nature = (array_key_exists($type, $nature_map)) ? $nature_map[$type] : $type;
            // now work out the length
            if ($nature == 'integer') {
                // if it's one of the ints, then look up the length in bytes
                $length = (array_key_exists($type, $int_bytes)) ? $int_bytes[$type] : 0;
            } elseif ($nature == 'character') {
                // if it's a character type then use the length that was in parentheses
                // eg if it was a varchar(25), we want the 25
                if (isset($row['CHARACTER_MAXIMUM_LENGTH'])) {
                    $length = $row['CHARACTER_MAXIMUM_LENGTH'];
                } // otherwise it could be any length (eg if it was a 'text')
                else {
                    $length = defined('PHP_INT_MAX') ? PHP_INT_MAX : 9999;
                }
            } else  // we're only dealing with a few simple cases at the moment
            {
                $length = NULL;
            }
            // Convert the is_nullable field to a boolean
            $is_nullable = (utf8_strtolower($row['IS_NULLABLE']) == 'yes') ? TRUE : FALSE;

            $fields[$i]['name'] = $name;
            $fields[$i]['type'] = $type;
            $fields[$i]['nature'] = $nature;
            $fields[$i]['length'] = $length;
            $fields[$i]['is_nullable'] = $is_nullable;
        }
        return $fields;
    }

}

/** mrbsGetRepeatEntryList
 *
 * Returns a list of the repeating entries
 *
 * $time          The start time
 * $enddate       When the repeat ends
 * $rep_details   An associative array containing the repeat details, indexed by
 *                  rep_type        What type of repeat is it
 *                  rep_opt         The repeat entries (if the repeat type is weekly then at least one
 *                                  repeat day must be set)
 *                  rep_num_weeks   The repeat frequency for weekly repeats
 *                  month_absolute  The repeat day of the month for monthly repeats
 * $n             Maximum number of entries to find
 *
 * Returns:
 *   empty     - The entry does not repeat
 *   an array  - This is a list of start times of each of the repeat entrys
 */
function mrbsGetRepeatEntryList($time, $enddate, $rep_details, $n)
{
    $entries = array();

    $date = getdate($time);

    $sec         = $date['seconds'];
    $min         = $date['minutes'];
    $hour        = $date['hours'];
    $day         = $date['mday'];
    $month       = $date['mon'];
    $year        = $date['year'];
    $start_day   = $date['wday'];
    $start_dom   = $day;  // the starting day of the month
    $start_month = $month;

    // Make sure that the first date is a member of the series
    switch($rep_details['rep_type'])
    {
        case REP_WEEKLY:
            for ($j=$start_day; ($j<7+$start_day) && !$rep_details['rep_opt'][$j%7]; $j++)
            {
                $day++;
            }
            break;
        case REP_MONTHLY:
            if (isset($rep_details['month_absolute']))
            {
                $day = $rep_details['month_absolute'];
                if ($day < $start_dom)
                {
                    $month++;
                }
            }
            elseif (isset($rep_details['month_relative']))
            {
                $day = byday_to_day($year, $month, $rep_details['month_relative']);
                while (($day === FALSE) || (($day < $start_dom) && ($month == $start_month)))
                {
                    $month++;
                    $day = byday_to_day($year, $month, $rep_details['month_relative']);
                }
            }
            else
            {
                trigger_error("No monthly repeat type, E_USER_WARNING");
            }
            trimToEndOfMonth($month, $day, $year);
            break;
        default:
            break;
    }

    for ($i = 0; $i < $n; $i++)
    {
        $time = mktime($hour, $min, $sec, $month, $day, $year);

        if ($time > $enddate)
        {
            break;
        }

        $entries[] = $time;

        switch($rep_details['rep_type'])
        {
            case REP_DAILY:
                $day++;
                break;

            case REP_WEEKLY:
                $j = $cur_day = date("w", $time);
                // Skip over days of the week which are not enabled:
                do
                {
                    $day++;
                    $j = ($j + 1) % 7;
                    // If we've got back to the beginning of the week, then skip
                    // over the weeks we've got to miss out (eg miss out one week
                    // if we're repeating every two weeks)
                    if ($j == $start_day)
                    {
                        $day += 7 * ($rep_details['rep_num_weeks'] - 1);
                    }
                }
                while (($j != $cur_day) && !$rep_details['rep_opt'][$j]);
                break;

            case REP_MONTHLY:
                do
                {
                    $month++;
                    if (isset($rep_details['month_absolute']))
                    {
                        // Get the day of the month back to where it should be (in case we
                        // decremented it to make it a valid date last time round)
                        $day = $rep_details['month_absolute'];
                    }
                    else
                    {
                        $day = byday_to_day($year, $month, $rep_details['month_relative']);
                    }
                } while ($day === FALSE);
                trimToEndOfMonth($month, $day, $year);
                break;

            case REP_YEARLY:
                // Get the day of the month back to where it should be (in case we
                // decremented it to make it a valid date last time round)
                $day = $start_dom;
                $year++;
                trimToEndOfMonth($month, $day, $year);
                break;

            // Unknown repeat option
            default:
                trigger_error("Unknown repeat type, E_USER_NOTICE");
                break;
        }
    }

    return $entries;
}

function trimToEndOfMonth(&$month, &$day, &$year)
{
    // Make the month valid so that we can use checkdate()
    while ($month > 12)
    {
        $month -= 12;
        $year++;
    }
    // Make the date valid if day is more than number of days in month:
    while (!checkdate($month, $day, $year) && ($day > 1))
    {
        $day--;
    }
}

// Makes bookings
//    $bookings     an array of bookings
//    $id           the id of the current booking when editing an existing entry
function mrbsMakeBookings($bookings, $id=NULL, $just_check=FALSE, $skip=FALSE, $original_room_id=NULL, $send_mail=FALSE, $edit_type='')
{
    global $max_rep_entrys, $enable_periods, $resolution, $mail_settings;
    //global $tbl_entry, $tbl_room, $tbl_area;
    $tbl_entry = 'times';
    // All the data, except for the status and room id, will be common
    // across the bookings
    $common = $bookings[0];
    // Work out the duration in seconds, but adjust it for DST changes so that
    // the user will still see, for example, "24 hours" when a booking goes from
    // 1200 one day to 1200 the next, crossing a DST boundary.
    $duration_seconds = $common['end_time'] - $common['start_time'];
    $duration_seconds -= cross_dst($common['start_time'], $common['end_time']);
    // Now get the duration, which will be needed for email notifications
    // (We do this before we adjust for DST so that the user sees what they expect to see)
    $duration = $duration_seconds;
    $date = getdate($common['start_time']);
    toTimeString($duration, $dur_units, FALSE);

    // Expand a series into a list of start times:
    if ($bookings[0]['rep_type'] != REP_NONE)
    {
        $rep_details = array();
        foreach (array('rep_type',
                     'rep_opt',
                     'rep_num_weeks',
                     'month_absolute',
                     'month_relative') as $key)
        {
            if (isset($common[$key]))
            {
                $rep_details[$key] = $common[$key];
            }
        }
        $reps = mrbsGetRepeatEntryList($common['start_time'],
            isset($common['end_date']) ? $common['end_date'] : 0,
            $rep_details,
            $max_rep_entrys);
    }

    // When checking for overlaps, for Edit (not New), ignore this entry and series:
    if (isset($id))
    {

        $ignore_id = $id;
        $repeat_id = get_repeat_id($id);
    }
    else {
        $ignore_id = 0;
        $repeat_id = NULL;
    }

    // Validate the booking for (a) conflicting bookings and (b) conformance to rules
    $valid_booking = TRUE;
    $conflicts = array();     // Holds a list of all the conflicts
    $rules_broken = array();  // Holds an array of the rules that have been broken
    $skip_lists = array();    // Holds a 2D array of bookings to skip past.  Indexed
    // by room id and start time
    // Check for any schedule conflicts in each room we're going to try and
    // book in;  also check that the booking conforms to the policy
    foreach ($bookings as $booking)
    {
        $skip_lists[$booking['room_id']] = array();
        if ($booking['rep_type'] != REP_NONE && !empty($reps))
        {
            if(count($reps) < $max_rep_entrys)
            {
                for ($i = 0; $i < count($reps); $i++)
                {
                    // calculate diff each time and correct where events
                    // cross DST
                    $diff = $duration_seconds;
                    $diff += cross_dst($reps[$i], $reps[$i] + $diff);

                    $this_booking = $booking;
                    $this_booking['start_time'] = $reps[$i];
                    $this_booking['end_time'] = $reps[$i] + $diff;
                    $tmp = mrbsCheckFree($this_booking, $ignore_id, $repeat_id);

                    $skip_this_booking = FALSE;
                    if (!empty($tmp))
                    {
                        // If we've been told to skip past existing bookings, then add
                        // this start time to the list of start times to skip past.
                        // Otherwise it's an invalid booking
                        if ($skip)
                        {
                            $skip_lists[$this_booking['room_id']][] = $this_booking['start_time'];
                            $skip_this_booking = TRUE;
                        }
                        else
                        {
                            $valid_booking = FALSE;
                        }
                        // In both cases remember the conflict data.   (We don't at the
                        // moment do anything with the data if we're skipping, but we might
                        // in the future want to display a list of bookings we've skipped past)
                        $conflicts = array_merge($conflicts, $tmp);
                    }
                    // If we're not going to skip past this booking, check that the booking
                    // conforms to the booking policy.  (If we're going to skip past this
                    // booking then it doesn't matter whether or not it conforms to the policy
                    // because we're never going to make it)
                    if (!$skip_this_booking)
                    {
                        //$errors = mrbsCheckPolicy($this_booking, $ignore_id, $repeat_id);
                        if (count($errors) > 0)
                        {
                            $valid_booking = FALSE;
                            $rules_broken = array_merge($rules_broken, $errors);
                        }
                    }
                } // for
            }
            else
            {
                $valid_booking = FALSE;
                $rules_broken[] = get_vocab("too_may_entrys");
            }
        }
        else
        {
            $tmp = mrbsCheckFree($booking, $ignore_id, 0);
            if (!empty($tmp))
            {
                $valid_booking = FALSE;
                $conflicts = array_merge($conflicts, $tmp);
            }
            // check that the booking conforms to the booking policy

            //$errors = mrbsCheckPolicy($booking, $ignore_id, 0);
            if (count($errors) > 0)
            {
                $valid_booking = FALSE;
                $rules_broken = array_merge($rules_broken, $errors);
            }
        }

    } // end foreach bookings

    // If we are editing an existing booking then we need to check that we are
    // allowed to delete it
    if (isset($id))
    {

        if ($edit_type == "series")
        {
            $old_bookings = get_bookings_in_series($repeat_id);
        }
        else
        {
            $old_bookings = array();
            $old_bookings[] = getBooking($id);
        }

        foreach ($old_bookings as $old_booking)
        {
            $errors = array();
            //mrbsCheckPolicy($old_booking, FALSE, FALSE, TRUE);
            if (count($errors) > 0)
            {
                $valid_booking = FALSE;
                $rules_broken = array_merge($rules_broken, $errors);
            }
        }

    }

    // Tidy up the lists of conflicts and rules broken, getting rid of duplicates
    $conflicts = array_values(array_unique($conflicts));
    $rules_broken = array_values(array_unique($rules_broken));

    $result = array();
    $result['valid_booking'] = $valid_booking;
    $result['rules_broken'] = $rules_broken;
    $result['conflicts'] = $conflicts;

    // If we've just been asked to check the bookings, or if it wasn't a valid
    // booking, then stop here and return the results

    // Otherwise we go on to commit the booking

    $new_details = array(); // We will pass this array in the Ajax result
    $rooms = array();
    foreach ($bookings as $booking)
    {
        $rooms[] = $booking['room_id'];
    }

    foreach ($bookings as $booking)
    {
        // We need to work out whether this is the original booking being modified,
        // because, if it is, we keep the ical_uid and increment the ical_sequence.
        // We consider this to be the original booking if there was an original
        // booking in the first place (in which case the original room id will be set) and
        //      (a) this is the same room as the original booking
        //   or (b) there is only one room in the new set of bookings, in which case
        //          what has happened is that the booking has been changed to be in
        //          a new room
        //   or (c) the new set of rooms does not include the original room, in which
        //          case we will make the arbitrary assumption that the original booking
        //          has been moved to the first room in the list and the bookings in the
        //          other rooms are clones and will be treated as new bookings.

        if (isset($original_room_id) &&
            (($original_room_id == $booking['room_id']) ||
                (count($rooms) == 1) ||
                (($rooms[0] == $booking['room_id']) && !in_array($original_room_id, $rooms))))
        {
            // This is an existing booking which has been changed.   Keep the
            // original ical_uid and increment the sequence number.
            $booking['ical_sequence']++;
        }
        else
        {
            // This is a new booking.   We generate a new ical_uid and start
            // the sequence at 0 - unless there already are uid and sequence
            // numbers, for example when importing an iCalendar file

            if (empty($booking['ical_uid']))
            {
                $booking['ical_uid'] = 1;
            }
            if (empty($booking['ical_sequence']))
            {
                $booking['ical_sequence'] = 0;
            }
        }

        if ($booking['rep_type'] == REP_NONE)
        {
            $booking['entry_type'] = (isset($repeat_id)) ? ENTRY_RPT_CHANGED : ENTRY_SINGLE;
            $booking['repeat_id'] = $repeat_id;
        }
        // Add in the list of bookings to skip
        if (!empty($skip_lists) && !empty($skip_lists[$booking['room_id']]))
        {
            $booking['skip_list'] = $skip_lists[$booking['room_id']];
        }
        // The following elements are needed for email notifications
        $booking['duration'] = $duration;
        $booking['dur_units'] = $dur_units;

        if ($booking['rep_type'] != REP_NONE)
        {
            $details = mrbsCreateRepeatingEntrys($booking);
            $new_id = $details['id'];
            $is_repeat_table = $details['series'];
            asort($details['start_times']);
            $result['start_times'] = $details['start_times'];
        }
        else
        {
            // Create the entry:
            $new_id = mrbsCreateSingleEntry($booking);
            $is_repeat_table = FALSE;
            $result['start_times'] = array($booking['start_time']);
        }
        $new_details[] = array('id' => $new_id, 'room_id' => $booking['room_id']);
        $booking['id'] = $new_id;  // Add in the id now we know it

        // Send an email if neccessary, provided that the entry creation was successful
        if ($send_mail && !empty($new_id))
        {
            // Only send an email if (a) this is a changed entry and we have to send emails
            // on change or (b) it's a new entry and we have to send emails for new entries
            if ((isset($id) && $mail_settings['on_change']) ||
                (!isset($id) && $mail_settings['on_new']))
            {
                require_once "functions_mail.inc";
                // Get room name and area name for email notifications.
                // Would be better to avoid a database access just for that.
                // Ran only if we need details
                if ($mail_settings['details'])
                {
                    $sql = "SELECT R.room_name, A.area_name
                    FROM $tbl_room R, $tbl_area A
                   WHERE R.id=${booking['room_id']} AND R.area_id = A.id
                   LIMIT 1";
                    $res = sql_query($sql);
                    $row = sql_mysqli_row_keyed($res, 0);
                    $booking['room_name'] = $row['room_name'];
                    $booking['area_name'] = $row['area_name'];
                }
                // If this is a modified entry then get the previous entry data
                // so that we can highlight the changes
                if (isset($id))
                {
                    if ($edit_type == "series")
                    {
                        $mail_previous = get_booking_info($repeat_id, TRUE);
                    }
                    else
                    {
                        $mail_previous = get_booking_info($id, FALSE);
                    }
                }
                else
                {
                    $mail_previous = array();
                }
                // Send the email
                notifyAdminOnBooking($booking, $mail_previous, !isset($id), $is_repeat_table, $result['start_times']);
            }
        }
    } // end foreach $bookings

    //sql_mutex_unlock("$tbl_entry");

    $result['new_details'] = $new_details;
    $result['slots'] = intval(($common['end_time'] - $common['start_time'])/$resolution);
    return $result;
}

/** mrbsCheckFree()
 *
 * Check to see if the time period specified is free
 *
 * $booking   - The booking in question - an associative array
 * $ignore    - An entry ID to ignore, 0 to ignore no entries
 * $repignore - A repeat ID to ignore everything in the series, 0 to ignore no series
 *
 * Returns:
 *   nothing   - The area is free
 *   something - An error occured, the return value is an array of conflicts
 */
function mrbsCheckFree(&$booking, $ignore, $repignore)
{
    //global $tbl_entry;
    global $enable_periods, $periods, $twentyfourhour_format;
    global $strftime_format, $is_private_field;
    $tbl_entry = 'times';
    $user= $booking['user'];
    //$user = getUserName();

    //get_area_settings(get_area($room_id));

    // Select any meetings which overlap for this room:
    $sql = "SELECT [id], [start_time]
            FROM $tbl_entry
           WHERE start_time < ${booking['end_time']}
             AND end_time > ${booking['start_time']}
             AND user = '$user'";

    if ($ignore > 0)
    {
        $sql .= " AND id <> $ignore";
    }
    if ($repignore > 0)
    {
        $sql .= " AND repeat_id <> $repignore";
    }
    $sql .= " ORDER BY start_time";

    $res = sql_query($sql);
    if (! $res)
    {
        // probably because the table hasn't been created properly
        trigger_error(sql_error(), E_USER_WARNING);
        fatal_error(TRUE, get_vocab("fatal_db_error"));
    }
    if (sql_count($res) == 0)
    {
        sqlsrv_free_stmt($res);
        return "";
    }
    // Get the room's area ID for linking to day, week, and month views:
    //$area = mrbsGetRoomArea($room_id);

    // Build an listing all the conflicts:
    $err = array();
    for ($i = 0; ($row = sql_mysqli_row_keyed($res, $i)); $i++)
    {
        $starts = getdate($row['start_time']);
        $param_ym = "area=$area&amp;year=$starts[year]&amp;month=".$starts['mon'];
        $param_ymd = $param_ym . "&amp;day=" . $starts['mday'];

        if ($enable_periods)
        {
            $p_num =$starts['minutes'];
            $startstr = utf8_strftime($strftime_format['date'] . ", ",
                    $row['start_time']) . $periods[$p_num];
        }
        else
        {
            $startstr = utf8_strftime(($twentyfourhour_format) ? $strftime_format['datetime24'] : $strftime_format['datetime12'],
                $row['start_time']);
        }


        // enclose  the viewday etc. links in a span to make it easier for JavaScript to strip them out
        $err[] = "<a href=\"view_entry.php?id=".$row['id']."\">".$row['name']."</a>"
            . " (" . $startstr . ") "
            . "<span>(<a href=\"temp.php?$param_ymd\">".get_vocab("viewday")."</a>"
            . " | <a href=\"temp.php?room=$room_id&amp;$param_ymd\">".get_vocab("viewweek")."</a>"
            . " | <a href=\"month.php?room=$room_id&amp;$param_ym\">".get_vocab("viewmonth")."</a>)</span>";
    }

    return $err;
}

function getBooking($id)
{
    global $tbl_entry;
    $tbl_entry = 'times';
    if (!isset($id))
    {
        return NULL;
    }

    $sql = "SELECT TOP 1 * FROM $tbl_entry WHERE id=$id";
    $res = sql_query($sql);

    if ($res === FALSE)
    {
        trigger_error(sql_error(), E_USER_WARNING);
        return FALSE;
    }

    if (sql_count($res) == 0)
    {
        return NULL;
    }

    return sql_mysqli_row_keyed($res, 0);
}

function get_bookings_in_series($repeat_id)
{
    global $tbl_entry;
    $tbl_entry = 'times';
    $bookings = array();

    $sql = "SELECT id FROM $tbl_entry WHERE repeat_id = $repeat_id";
    $res = sql_query_array($sql);
    if ($res === FALSE)
    {
        trigger_error(sql_error(), E_USER_WARNING);
    }
    foreach ($res as $id)
    {
        $bookings[] = getBooking($id);
    }
    return $bookings;
}

// Run an SQL query that returns a simple one dimensional array of results.
// The SQL query must select only one column.   Returns an empty array if
// no results, or FALSE if there's an error
function sql_query_array($sql)
{

    $res = sql_query($sql);

    if ($res === FALSE)
    {
        return FALSE;
    }
    else
    {
        $result = array();
        for ($i = 0; ($row = sql_row($res, $i)); $i++)
        {
            $result[] = $row[0];
        }
        return $result;
    }
}

/** mrbsCreateRepeatingEntrys()
 *
 * Creates a repeat entry in the data base + all the repeating entrys
 *
 * $data      - An array containing the entry details
 *
 * Returns:
 *   an array
 *   ['id']          - 0 if an error occurred or if no bookings could be
 *                     made, otherwise an id
 *   ['series']      - boolean: TRUE if the id refers to the repeat table
 *                              FALSE if the id refers to the entry table
 *   ['start_times'] - an array of start times that have been created
 *
 */
function mrbsCreateRepeatingEntrys($data)
{
    global $max_rep_entrys;

    $result = array('id' => 0, 'series' => FALSE, 'start_times' => array());

    if (!isset($data['skip_list']))
    {
        $data['skip_list'] = array();
    }

    $rep_details = array();
    foreach (array('rep_type',
                 'rep_opt',
                 'rep_num_weeks',
                 'month_absolute',
                 'month_relative') as $key)
    {
        if (isset($data[$key]))
        {
            $rep_details[$key] = $data[$key];
        }
    }

    $reps = mrbsGetRepeatEntryList($data['start_time'],
        $data['end_date'],
        $rep_details,
        $max_rep_entrys);

    // Don't make any bookings if we've been asked to book up
    // more entries than we are allowed in a single repeat.
    if (count($reps) > $max_rep_entrys)
    {
        $result['id'] = 0;
        return $result;
    }

    // If $reps is empty, then this is a single booking, so treat
    // it differently
    if (empty($reps))
    {
        $data['entry_type'] = ENTRY_SINGLE;
        $data['repeat_id'] = NULL;
        $id = mrbsCreateSingleEntry($data);
        $result['id'] = $id;
        $result['series'] = FALSE;
        $result['start_times'][] = $data['start_time'];
        return $result;
    }

    // This is a repeat booking.   If we've got to skip past all
    // the entries, then don't make a booking!
    if (count($reps) == count($data['skip_list']))
    {
        $result['id'] = 0;
        return $result;
    }

    // Maybe one should also consider adjusting the start_time for
    // the repeat if the first (or more) entries of the series are
    // to be skipped.    However I haven't done so here and it gives the
    // maybe slightly strange result that the start date of the series won't
    // have an entry on that date.   But then this is no different from
    // the way MRBS works at present if you create a series and then
    // delete the first entry.
    //
    // Note also that RFC 5545 allows this behaviour in 3.8.5.1:
    //   'The "EXDATE" property can be used to exclude the value specified
    //    in "DTSTART".  However, in such cases, the original "DTSTART" date
    //    MUST still be maintained by the calendaring and scheduling system
    //    because the original "DTSTART" value has inherent usage
    //    dependencies by other properties such as the "RECURRENCE-ID".'

    $id = mrbsCreateRepeatEntry($data);
    if ($id)
    {
        $data['entry_type'] = ENTRY_RPT_ORIGINAL;
        $data['repeat_id'] = $id;
        $start_time = $data['start_time'];
        $end_time = $data['end_time'];
        for ($i = 0; $i < count($reps); $i++)
        {
            // Provided this isn't one of the entries to skip, go ahead
            // and make the booking
            if (!in_array($reps[$i], $data['skip_list']))
            {
                // calculate diff each time and correct where events
                // cross DST
                $diff = $end_time - $start_time;
                $diff += cross_dst($reps[$i], $reps[$i] + $diff);
                $data['start_time'] = $reps[$i];
                $data['end_time'] = $reps[$i] + $diff;
                $ent_id = mrbsCreateSingleEntry($data);
                $result['start_times'][] = $data['start_time'];
            }
        }
    }
    $result['id'] = $id;
    $result['series'] = TRUE;
    return $result;
}

/** mrbsCreateSingleEntry()
 *
 * Create a single (non-repeating) entry in the database
 *
 * $data      - An array containing the entry details
 *
 * Returns:
 *   0        - An error occurred while inserting the entry
 *   non-zero - The entry's ID
 */
function mrbsCreateSingleEntry($data)
{
    global $tbl_entry;
    $tbl_entry = 'times';
    // make sure that any entry is of a positive duration
    // this is to trap potential negative duration created when DST comes
    // into effect
    if ($data['end_time'] > $data['start_time'])
    {
        // If we're about to create an individual member of a series for the first time
        // then give it a recurrence-id equivalent to the start time.  It should always
        // keep this value, even if the start time is subsequently changed.
        if ($data['entry_type'] == ENTRY_RPT_ORIGINAL)
        {
            $data['ical_recur_id'] = gmdate(RFC5545_FORMAT . '\Z', $data['start_time']);
        }
        $result = mrbsCreateEntry($tbl_entry, $data);
        return $result;
    }
    else
    {
        return 0;
    }
}

/** mrbsCreateRepeatEntry()
 *
 * Creates a repeat entry in the data base
 *
 * $data      - An array containing the entry details
 *
 * Returns:
 *   0        - An error occurred while inserting the entry
 *   non-zero - The entry's ID
 */
function mrbsCreateRepeatEntry($data)
{
    global $tbl_repeat;
    $tbl_repeat = 'repeats';
    $result = mrbsCreateEntry($tbl_repeat, $data);
    return $result;
}

/** mrbsCreateEntry()
 *
 * Create an entry in the database
 *
 * $table         - The table in which to create the entry
 * $data          - An array containing the row data for the entry
 *
 * Returns:
 *   0        - An error occurred while inserting the entry
 *   non-zero - The entry's ID
 */
function mrbsCreateEntry($table, $data)
{
    global $standard_fields, $db_tbl_prefix;

    $sql_col = array();
    $sql_val = array();
    $table_no_prefix = utf8_substr($table, utf8_strlen($db_tbl_prefix));  // strip the prefix off the table name

    $fields = sql_field_info($table);
    foreach ($fields as $field)
    {
        $key = $field['name'];
        // If the key doesn't exist in the $data array then the database will just use its
        // default value for the column.    If it does exist and is set to NULL then we'll
        // write NULL to the database (which may not necessarily be the default value).
        if (array_key_exists($key, $data))
        {
            switch ($key)
            {
                // integers
                case 'start_time':
                case 'end_time':
                case 'entry_type':
                case 'repeat_id':
                case 'rep_type':
                case 'month_absolute':
                case 'end_date':
                case 'room_id':
                case 'status':
                case 'ical_sequence':
                    $sql_col[] = $key;
                    $sql_val[] = (isset($data[$key])) ? (int)$data[$key] : 'NULL';
                    break;

                // strings
                case 'create_by':
                case 'modified_by':
                case 'name':
                case 'type':
                case 'description':
                case 'month_relative':
                case 'ical_uid':
                case 'ical_recur_id':
                    $sql_col[] = $key;
                    $sql_val[] = (isset($data[$key])) ? "'" . sql_escape($data[$key]) . "'" : 'NULL';
                    break;

                // special case - rep_opt
                case 'rep_opt':
                    // pgsql doesn't like empty strings
                    $sql_col[] = $key;
                    $sql_val[] = (empty($data[$key])) ? "'0'" : "'" . sql_escape($data[$key]) . "'";
                    break;

                // special case - rep_num_weeks
                case 'rep_num_weeks':
                    if (!empty($data[$key]))
                    {
                        $sql_col[] = $key;
                        $sql_val[] = $data[$key];
                    }
                    break;

                default:
                    // custom fields
                    if (!in_array($key, $standard_fields[$table_no_prefix]))
                    {
                        $sql_col[] = $key;

                        // Depending on the nature of the custom field the treatment will vary
                        switch ($field['nature'])
                        {
                            case 'integer':
                                if (!isset($data[$key]) || ($data[$key] === ''))
                                {
                                    // Try and set it to NULL when we can because there will be cases when we
                                    // want to distinguish between NULL and 0 - especially when the field
                                    // is a genuine integer.
                                    $value = ($field['is_nullable']) ? 'NULL' : 0;
                                }
                                else
                                {
                                    $value = (int)$data[$key];
                                }
                                break;
                            default:
                                if (!isset($data[$key]))
                                {
                                    $value = ($field['is_nullable']) ? 'NULL' : '';
                                }
                                else
                                {
                                    $value = "'" . sql_escape($data[$key]) . "'";
                                }
                                break;
                        } // switch ($field_natures[$key])

                        $sql_val[] = $value;
                    }
                    // do nothing for fields that aren't custom or otherwise listed above
                    break;

            } // switch ($key)
        } // if
    } // foreach

    $sql_col = array_map('sql_quote', $sql_col);
    $sql = "INSERT INTO $table (" . implode(', ', $sql_col) . ") VALUES (" . implode(', ',$sql_val) . ")";
    $sql .= "; SELECT SCOPE_IDENTITY() AS IDENTITY_COLUMN_NAME";
    $res = sql_query($sql);
    $ret = sqlsrv_rows_affected($res);
    if ($ret < 0)
    {
        // probably because the table hasn't been created properly
        trigger_error(sql_error(), E_USER_WARNING);
        fatal_error(TRUE, get_vocab("fatal_db_error"));
    }

    return lastInsertID($res);
}

// Escapes special characters in a string for use in an SQL statement
function sql_escape($str)
{
    return $str;
}

// Run an SQL query that doesn't produce results
function sql_command($sql)
{
    $ret = -1;
    $res = sql_query($sql);
    if ($res)
    {
        $ret = sqlsrv_rows_affected($res);
    }
    return $ret;
}

// SQL server function to get last inserted ID
function lastInsertID($queryID)
{
    sqlsrv_next_result($queryID);
    sqlsrv_fetch($queryID);
    return sqlsrv_get_field($queryID, 0);
}

// Returns the results in an array with keys the same as the table
// field names.  In the event of an error stops with a fatal error,
// unless $silent is TRUE, when it returns FALSE.
function get_booking_info($id, $series, $silent=FALSE)
{
    global $tbl_entry, $tbl_repeat, $tbl_room, $tbl_area;
    $tbl_entry = 'times';
    $tbl_repeat= 'repeats';
    // Check that we've got an id
    if (!isset($id))
    {
        trigger_error("id not set", E_USER_WARNING);
        if ($silent)
        {
            return FALSE;
        }
        else
        {
            fatal_error(TRUE, ($series ? get_vocab("invalid_series_id") : get_vocab("invalid_entry_id")));
        }
    }

    $table = ($series) ? $tbl_repeat : $tbl_entry;
    $table_fields = sql_field_info($table);
    // Build an array of the field names in the repeat table so that
    // we'll be able to do some sanity checking later
    $repeat_fields = sql_field_info($tbl_repeat);
    $rep_fields = array();
    foreach ($repeat_fields as $field)
    {
        $rep_fields[$field['name']] = 1;
    }

    $terms = array("(end_time - start_time) AS duration");

    foreach ($table_fields as $field)
    {
        switch ($field['name'])
        {
            // these fields only exist in the entry table
            case 'entry_type':
            case 'repeat_id':
            case 'ical_recur_id':
                array_push($terms, $field['name']);
                break;

            case 'info_time':
            case 'info_user':
            case 'info_text':
                if ($series)
                {
                    array_push($terms, $field['name']);
                }
                else
                {
                    array_push($terms, $field['name']);
                }
                break;

            default:
                // These are (a) all the standard fields which are common to the entry
                // and repeat tables and (b) all the custom fields, which should be
                // common to the two tables (we will do a check to make sure)
                if (!$series && !array_key_exists($field['name'], $rep_fields))
                {
                    // If this is the entry table then check that the custom field also
                    // exists in the repeat table
                    fatal_error(0, "Custom fields problem, '".$field['name']."' exists in entry table but not in repeat table");
                }

                array_push($terms, $field['name']);
                break;
        }
    }
    $sql = "SELECT " . implode(", ",$terms)."
            FROM $table
             WHERE id=$id";
    $res = sql_query($sql);
    if (! $res)
    {
        trigger_error(sql_error(), E_USER_WARNING);
        if ($silent)
        {
            return FALSE;
        }
        else
        {
            fatal_error(FALSE, get_vocab("fatal_db_error"));
        }
    }

    if (sql_count($res) < 1)
    {
        // It's quite possible that the id will have disappeared, eg if somebody
        // else has deleted or edited the entry, or if the back button is pressed
        // after deleting an entry.
        if ($silent)
        {
            return FALSE;
        }
        else
        {
            fatal_error(TRUE, ($series ? get_vocab("invalid_series_id") : get_vocab("invalid_entry_id")));
        }
    }

    $row = sql_mysqli_row_keyed($res, 0);
    sqlsrv_free_stmt($res);

    // Now get the duration.
    // Don't translate the units at this stage.   We'll translate them later.
    $d = get_duration($row['start_time'], $row['end_time'], $row['enable_periods'], FALSE);
    $row['duration'] = $d['duration'];
    $row['dur_units'] = $d['dur_units'];
    // Get some extra information
    if ($series)
    {
        $row['entry_info_time'] = '';
        $row['entry_info_user'] = '';
        $row['entry_info_text'] = '';
    }
    else
    {
        // Get the repeat information
        if (!isset($row['repeat_id']))
        {
            $row['rep_type'] = REP_NONE;   // just as a precaution
            $row['repeat_info_time'] = '';
            $row['repeat_info_user'] = '';
            $row['repeat_info_text'] = '';
        }
        else
        {
            $res = sql_query("SELECT TOP 1 rep_type, end_date, rep_opt, rep_num_weeks, month_absolute, month_relative,
                        info_time AS repeat_info_time, info_user AS repeat_info_user, info_text AS repeat_info_text
                        FROM $tbl_repeat WHERE id=${row['repeat_id']}");
            if (!$res || (!$extra_row = sql_mysqli_row_keyed($res, 0)))
            {
                if (!$res)
                {
                    trigger_error(sql_error(), E_USER_WARNING);
                }
                if ($silent)
                {
                    return FALSE;
                }
                else
                {
                    fatal_error(TRUE, get_vocab("invalid_series_id"));
                }
            }
            $row['rep_type']         = $extra_row['rep_type'];
            $row['end_date']         = $extra_row['end_date'];
            $row['rep_opt']          = $extra_row['rep_opt'];
            $row['rep_num_weeks']    = $extra_row['rep_num_weeks'];
            $row['month_absolute']   = $extra_row['month_absolute'];
            $row['month_relative']   = $extra_row['month_relative'];
            $row['repeat_info_time'] = $extra_row['repeat_info_time'];
            $row['repeat_info_user'] = $extra_row['repeat_info_user'];
            $row['repeat_info_text'] = $extra_row['repeat_info_text'];
            sql_free($res);
        }
    }

    return $row;
}

// Gets the repeat_id for an entry in the entry table with id $entry_id
// Returns the repeat_id or NULL
function get_repeat_id($entry_id)
{
    global $tbl_entry;
    $tbl_entry = 'times';

    $sql = "SELECT TOP 1 repeat_id
            FROM $tbl_entry
           WHERE id=$entry_id";
    $res = sql_query($sql);

    if ($res === FALSE)
    {
        trigger_error(sql_error(), E_USER_WARNING);
        fatal_error(FALSE, get_vocab("fatal_db_error"));
    }
    if (sql_count($res) == 0)
    {
        // This should not happen
        trigger_error('$entry_id=' . "$entry_id does not exist.", E_USER_NOTICE);
        return NULL;
    }
    $row = sql_mysqli_row_keyed($res, 0);
    return $row['repeat_id'];
}

/** mrbsDelEntry()
 *
 * Delete an entry, or optionally all entries.   Will also delete any newly
 * orphaned rows in the repeat table.
 *
 * $user   - Who's making the request
 * $id     - The entry to delete
 * $series - If set, delete the series, except user modified entries
 * $all    - If set, include user modified entries in the series delete
 *
 * Returns FALSE if an error occurred, otherwise an array of start_times that
 * have been deleted.
 *
 */
function mrbsDelEntry($user, $id, $series, $all)
{
    global $tbl_entry, $tbl_repeat;
    $tbl_entry = 'times';
    $tbl_repeat = 'repeats';
    $start_times = array();

    // Get the repeat_id and room_id for this entry
    $res = sql_query("SELECT TOP 1 repeat_id, user FROM $tbl_entry WHERE id=$id");
    if (($res === FALSE) || (sql_count($res) <= 0))
    {
        return FALSE;
    }
    $row = sql_mysqli_row_keyed($res, 0);
    $repeat_id = $row['repeat_id'];
    $sql = "SELECT start_time, end_time, id, type FROM $tbl_entry WHERE ";

    if ($series)
    {
        $sql .= "repeat_id=$repeat_id";
    }
    else
    {
        $sql .= "id=$id";
    }
    $res = sql_query($sql);

    for ($i = 0; ($row = sql_mysqli_row_keyed($res, $i)); $i++)
    {
        /*    if(!getWritable($row['create_by'], $user, $room_id))
            {
              continue;
            }

            if ($series && $row['entry_type'] == ENTRY_RPT_CHANGED && !$all)
            {
              continue;
            }*/

        // check that the booking policies allow us to delete this entry
        /*$tmp = mrbsCheckPolicy($row, 0, 0, TRUE);
        if (!empty($tmp))
        {
          continue;
        }*/
        if (sql_command("DELETE FROM $tbl_entry WHERE id=" . $id) > 0)
        {
            $start_times[] = $row['start_time'];
        }
    }

    // Get rid of any orphaned rows in the repeat table
    if (!empty($repeat_id) &&
        sql_mysqli_query1("SELECT COUNT(*) FROM $tbl_entry WHERE repeat_id=$repeat_id") == 0)
    {
        sql_command("DELETE FROM $tbl_repeat WHERE id=$repeat_id");
    }

    asort($start_times);
    return $start_times;
}


function sql_quote($identifier)
{
    return "[" . $identifier . "]";
}

