<?php
/**
 * Created by PhpStorm.
 * User: tanni
 * Date: 1/8/2016
 * Time: 12:38 PM
 */

// A PHP version independent version of mktime().   (The $is_dst parameter
// became deporecated at PHP 5.1.0)
set_vocab('en');

// Set $vocab.   Returns TRUE if a lang file is found, otherwise FALSE
function set_vocab($lang)
{
    global $vocab_override, $cli_mode;
    global $PHP_SELF;
    global $vocab, $mrbs_admin, $mrbs_company;  // Used in lang files

    $lang = utf8_strtolower($lang);

    if (file_exists('lang'))
    {
        $lang_file = "lang/lang.$lang";
    }
    else
    {
        // The JavaScript files in the js directory end up including language.inc.
        // In this case we need to adjust the path
        $lang_file = "../lang/lang.$lang";
    }
    // When in CLI mode, we need to add the full path name as
    // file_exists() ignores the include path
    if ($cli_mode)
    {
        $lang_file = dirname($PHP_SELF) . "/" . $lang_file;
    }
    if (file_exists($lang_file))
    {
        // Get the standard language tokens
        include "$lang_file";
        // Apply any site overrides
        if (isset($vocab_override[$lang]))
        {
            foreach ($vocab_override[$lang] as $tag => $str)
            {
                $vocab[$tag] = $str;
            }
        }
        return TRUE;
    }
    return FALSE;
}

// Returns TRUE if the time $hm1 is before $hm2
// $hm1 and $hm2 are associative arrays indexed by 'hours' and 'minutes'.
// The indices are chosen to allow the result of the PHP getdate() function
// to be passed as parameters

// returns the modification (in seconds) necessary on account of any DST
// transitions when going from $start to $end
function cross_dst($start, $end)
{
    global $timezone;

    // Ideally we calculate the modification using the DateTimeZone information
    // in PHP, because not all DST transitions are 1 hour.  For example, Lord Howe
    // Island in Australia has a 30 minute transition
    if (class_exists('DateTimeZone'))
    {
        $thisDateTimeZone = new DateTimeZone($timezone);
        $startDateTime = new DateTime(date('c', $start), $thisDateTimeZone);
        $endDateTime = new DateTime(date('c', $end), $thisDateTimeZone);
        $modification = $startDateTime->getOffset() - $endDateTime->getOffset();
    }
    // Otherwise we have to assume that the transition is 1 hour.
    else
    {
        // entering DST
        if (!date( "I", $start) &&  date( "I", $end))
        {
            $modification = -SECONDS_PER_HOUR;
        }
        // leaving DST
        else if (date( "I", $start) && !date( "I", $end))
        {
            $modification = SECONDS_PER_HOUR;
        }
        else
        {
            $modification = 0;
        }
    }

    return $modification;
}

// Determines whether there's a possibility that the interval between the two Unix
// timestamps could contain nominal times that don't exist, for example from 0100 to
// 0159 in Europe/London when entering DST.
function is_possibly_invalid($start, $end)
{
    // We err on the side of caution by widening the interval by a day at each end.   This
    // allows for the possibility that the start or end times have been calculated by using
    // mktime() on an invalid time!
    return (cross_dst($start - 86400, $end + 86400) < 0);
}


function hm_before($hm1, $hm2)
{
    return ($hm1['hours'] < $hm2['hours']) ||
    (($hm1['hours'] == $hm2['hours']) && ($hm1['minutes'] < $hm2['minutes']));
}

function mrbs_mktime($hour, $minute, $second, $month, $day, $year)
{
    if (version_compare(PHP_VERSION, '5.1.0') >= 0)
    {
        return mktime($hour, $minute, $second, $month, $day, $year);
    }
    else
    {
        return mktime($hour, $minute, $second, $month, $day, $year,
            is_dst($month, $day, $year, $hour));
    }
}

// Gets the UNIX timestamp for the start of the first slot on the given day
function get_start_first_slot($month, $day, $year)
{
    global $morningstarts, $morningstarts_minutes, $enable_periods;

    if ($enable_periods)
    {
        return mrbs_mktime(12, 0, 0, $month, $day, $year);
    }

    $t = mrbs_mktime($morningstarts, $morningstarts_minutes, 0,
        $month, $day, $year);
    return $t;
}


// Gets the UNIX timestamp for the start of the last slot on the given day
function get_start_last_slot($month, $day, $year)
{
    global $morningstarts, $morningstarts_minutes, $eveningends, $eveningends_minutes, $enable_periods, $periods;

    if ($enable_periods)
    {
        return mrbs_mktime(12, count($periods) -1, 0, $month, $day, $year);
    }

    // Work out if $evening_ends is really on the next day
    if (hm_before(array('hours' => $eveningends, 'minutes' => $eveningends_minutes),
        array('hours' => $morningstarts, 'minutes' => $morningstarts_minutes)))
    {
        $day++;
    }
    $t = mrbs_mktime($eveningends, $eveningends_minutes, 0,
        $month, $day, $year);
    return $t;
}

function get_area_name($user, $all=FALSE)
{
    global $conn;
    $sql = "SELECT TOP 1 name
            FROM users
           WHERE code='$user'";
    if (empty($all))
    {
        $sql .= " AND disabled=0";
    }
    $params = array();
    $options = array("Scrollable"=>SQLSRV_CURSOR_KEYSET);
    $res = sqlsrv_query($conn, $sql, $params, $options);


    if ($res === FALSE)
    {
        trigger_error(sql_error(), E_USER_WARNING);
        return FALSE;
    }
    if (sqlsrv_num_rows($res) == 0)
    {
        return NULL;
    }

    $row = sql_row($res, 0);
    return $row;
}

// Remove backslash-escape quoting if PHP is configured to do it with
// magic_quotes_gpc. Use this whenever you need the actual value of a GET/POST
// form parameter (which might have special characters) regardless of PHP's
// magic_quotes_gpc setting.
function unslashes($s)
{
    if (get_magic_quotes_gpc())
    {
        return stripslashes($s);
    }
    else
    {
        return $s;
    }
}

function get_vocab($tag)
{
    global $vocab;

    // Return the tag itself if we can't find a vocab string
    if (!isset($vocab[$tag]))
    {
        return $tag;
    }

    $args = func_get_args();
    $args[0] = $vocab[$tag];
    return call_user_func_array('sprintf', $args);
}

// returns the numeric day of the week (0-6) in terms of the MRBS week as defined by
// $weekstarts.   For example if $weekstarts is set to 2 (Tuesday) and a $time for
// a Wednesday is given, then 1 is returned.
function day_of_MRBS_week($time)
{
    global $weekstarts;

    return (date('w', $time) - $weekstarts + 7) % 7;
}

function week_table_innerhtml($day, $month, $year, $user, $timetohighlight=NULL)
{
    global $enable_periods, $periods;
    global $times_along_top, $row_labels_both_sides, $column_labels_both_ends;
    global $resolution, $morningstarts, $morningstarts_minutes, $eveningends, $eveningends_minutes;
    global $weekstarts, $strftime_format;
    global $first_last_width, $column_hidden_width, $hidden_days;
    global $sql_mysqli_conn;
    // Check that we've got a valid, enabled room
    $sql = "SELECT COUNT(*)
            FROM users
           WHERE code='$user'
             AND disabled=0";

    $n_users = sql_mysqli_query1($sql, $sql_mysqli_conn);
    if (($n_users < 0) || ($n_users > 1))
    {
        if ($n_users < 0)
        {
            // SQL error, probably because the tables haven't been created
            trigger_error(sql_error(), E_USER_WARNING);
        }
        else
        {
            // Should never happen
            trigger_error("Internal error: multiple rooms with same id", E_USER_WARNING);
        }
        fatal_error(FALSE, get_vocab("fatal_db_error"));
    }
    if ($n_users == 0)
    {
        // No rooms have been created yet, or else they are all disabled
        // Add an 'empty' data flag so that the JavaScript knows whether this is a real table or not
        return "<tbody data-empty=1><tr><td><h1>".get_vocab("no_rooms_for_area")."</h1></td></tr></tbody>";
    }

    // We have a valid room
    $num_of_days=7; // days in a week


    // ensure that $morningstarts_minutes defaults to zero if not set
    if (!isset($morningstarts_minutes))
    {
        $morningstarts_minutes = 0;
    }

    if ($enable_periods)
    {
        $resolution = 60;
        $morningstarts = 12;
        $morningstarts_minutes = 0;
        $eveningends = 12;
        $eveningends_minutes = count($periods) - 1;
    }

    // Calculate how many days to skip back to get to the start of the week
    $time = mktime(12, 0, 0, $month, $day, $year);
    $skipback = day_of_MRBS_week($time);
    $day_start_week = $day - $skipback;
    // We will use $day for links and $day_start_week for anything to do with showing the bookings,
    // because we want the booking display to start on the first day of the week (eg Sunday if $weekstarts is 0)
    // but we want to preserve the notion of the current day (or 'sticky day') when switching between pages

    // Define the start and end of each day of the week in a way which is not
    // affected by daylight saving...
    for ($j = 0; $j<=($num_of_days-1); $j++)
    {
        $am7[$j] = get_start_first_slot($month, $day_start_week+$j, $year);
        $pm7[$j] = get_start_last_slot($month, $day_start_week+$j, $year);
        // Work out whether there's a possibility that a time slot is invalid,
        // in other words whether the booking day includes a transition into DST.
        // If we know that there's a transition into DST then some of the slots are
        // going to be invalid.   Knowing whether or not there are possibly invalid slots
        // saves us bothering to do the detailed calculations of which slots are invalid.
        $is_possibly_invalid[$j] = !$enable_periods && is_possibly_invalid($am7[$j], $pm7[$j]);
    }
    unset($j);  // Just so that we pick up any accidental attempt to use it later

    // Get all appointments for this week in the room that we care about.
    //
    // row['room_id'] = Room ID
    // row['start_time'] = Start time
    // row['end_time'] = End time
    // row['type'] = Entry type
    // row['name'] = Entry name (brief description)
    // row['entry_id'] = Entry ID
    // row['entry_description'] = Complete description
    // row['status'] = status code
    // row['entry_create_by'] = User who created entry
    // This data will be retrieved day-by-day

    $week_map = array();

    for ($j = 0; $j<=($num_of_days-1) ; $j++)
    {
        $sql = "SELECT [user], [start_time], [end_time], [type], [number], [repeat_id],
                   [id] AS entry_id
              FROM worklog.dbo.times
             WHERE [user] = '$user'
               AND [start_time] <= $pm7[$j] AND [end_time] > $am7[$j]
          ORDER BY [start_time]";   // necessary so that multiple bookings appear in the right order
        // Each row returned from the query is a meeting. Build an array of the
        // form:  $week_map[room][weekday][slot][x], where x = id, color, data, long_desc.
        // [slot] is based at 000 (HHMM) for midnight, but only slots within
        // the hours of interest (morningstarts : eveningends) are filled in.
        // [id], [data] and [long_desc] are only filled in when the meeting
        // should be labeled,  which is once for each meeting on each weekday.
        // Note: weekday here is relative to the $weekstarts configuration variable.
        // If 0, then weekday=0 means Sunday. If 1, weekday=0 means Monday.
        $res = sql_query($sql);
        if (! $res)
        {

            trigger_error(sql_error(), E_USER_WARNING);
            fatal_error(TRUE, get_vocab("fatal_db_error"));
        }
        else
        {

            for ($i = 0; ($row = sql_mysqli_row_keyed($res, $i)); $i++)
            {
                map_add_booking($row, $week_map[0][$j], $am7[$j], $pm7[$j]);
            }
        }
    }  // for ($j = 0; ...
    unset($j);  // Just so that we pick up any accidental attempt to use it later
    // START DISPLAYING THE MAIN TABLE
    $n_time_slots = get_n_time_slots();
    $morning_slot_seconds = (($morningstarts * 60) + $morningstarts_minutes) * 60;
    $evening_slot_seconds = $morning_slot_seconds + (($n_time_slots - 1) * $resolution);

    // TABLE HEADER
    $thead = "<thead>\n";
    $header_inner = "<tr>\n";

    $dformat = "%a<br>" . $strftime_format['daymonth'];
    // If we've got a table with times along the top then put everything on the same line
    // (ie replace the <br> with a space).   It looks slightly better
    if ($times_along_top)
    {
        $dformat = preg_replace("/<br>/", " ", $dformat);
    }

    // We can display the table in two ways
    if ($times_along_top)
    {
        // with times along the top and days of the week down the side
        $first_last_html = '<th class="first_last" style="width: ' . $first_last_width . '%">' .
            get_vocab('date') . ":</th>\n";
        $header_inner .= $first_last_html;

        $column_width = get_main_column_width($n_time_slots);

        for ($s = $morning_slot_seconds;
             $s <= $evening_slot_seconds;
             $s += $resolution)
        {
            // Put the seconds since the start of the day (nominal, not adjusted for DST)
            // into a data attribute so that it can be picked up by JavaScript
            $header_inner .= "<th data-seconds=\"$s\" style=\"width: $column_width%\">";
            // We need the span so that we can apply some padding.   We can't apply it
            // to the <th> because that is used by jQuery.offset() in resizable bookings
            $header_inner .= "<span>";
            if ( $enable_periods )
            {
                $header_inner .= period_name($s);
            }
            else
            {
                $header_inner .= hour_min($s);
            }
            $header_inner .= "</span>";
            $header_inner .= "</th>\n";
        }
        // next: line to display times on right side
        if (!empty($row_labels_both_sides))
        {
            $header_inner .= $first_last_html;
        }
    } // end "times_along_top" view (for the header)

    else
    {
        // the standard view, with days along the top and times down the side
        $first_last_html = '<th class="first_last" style="width: ' . $first_last_width . '%">' .
            ($enable_periods ? get_vocab('period') : get_vocab('time')) . ':</th>';
        $header_inner .= $first_last_html;

        $column_width = get_main_column_width($num_of_days, count($hidden_days));

        for ($j = 0; $j<=($num_of_days-1) ; $j++)
        {
            $t = mktime(12, 0, 0, $month, $day_start_week+$j, $year);
            $date = date('Y-n-d', $t);

            if (is_hidden_day(($j + $weekstarts) % 7))
            {
                // These days are to be hidden in the display (as they are hidden, just give the
                // day of the week in the header row

                $style = ($column_hidden_width == 0) ? 'display: none' : 'width: ' . $column_hidden_width . '%';
                $header_inner .= '<th class="hidden_day" style="' . $style . '">' .
                    utf8_strftime($strftime_format['dayname_cal'], $t) .
                    "</th>\n";
            }
            else
            {
                // Put the date into a data attribute so that it can be picked up by JavaScript
                $header_inner .= '<th data-date="' . $date . '" style="width: ' . $column_width . '%>' .
                    '<a href="day.php?year=' . strftime("%Y", $t) .
                    '&amp;month=' . strftime("%m", $t) . '&amp;day=' . strftime('%d', $t) .
                    '&amp;area=' . $area . ' title="' . get_vocab('viewday') . '">' .
                    utf8_strftime($dformat, $t) . "</a></th>\n";
            }
        }  // for ($j = 0 ...
        unset($j);  // Just so that we pick up any accidental attempt to use it later
        // next line to display times on right side
        if (!empty($row_labels_both_sides))
        {
            $header_inner .= $first_last_html;
        }
    }  // end standard view (for the header)

    $header_inner .= "</tr>\n";
    $thead .= $header_inner;
    $thead .= "</thead>\n";

    // Now repeat the header in a footer if required
    $tfoot = ($column_labels_both_ends) ? "<tfoot>\n$header_inner</tfoot>\n" : '';

    // TABLE BODY LISTING BOOKINGS
    $tbody = "<tbody>\n";
    // URL for highlighting a time. Don't use REQUEST_URI or you will get
    // the timetohighlight parameter duplicated each time you click.
    $base_url="test.php?year=$year&amp;month=$month&amp;day=$day&amp;user=$user";
    $row_class = "even_row";

    // We can display the table in two ways
    if ($times_along_top)
    {
        // with times along the top and days of the week down the side
        // See note above: weekday==0 is day $weekstarts, not necessarily Sunday.
        for ($thisday = 0; $thisday<=($num_of_days-1) ; $thisday++, $row_class = ($row_class == "even_row")?"odd_row":"even_row")
        {
            if (is_hidden_day(($thisday + $weekstarts) % 7))
            {
                // These days are to be hidden in the display: don't display a row
                // Toggle the row class back to keep it in sequence
                $row_class = ($row_class == "even_row")?"odd_row":"even_row";
                continue;
            }

            else
            {
                $tbody .= "<tr class=\"$row_class\">\n";

                $wt = mktime( 12, 0, 0, $month, $day_start_week+$thisday, $year );
                $wday = date("d", $wt);
                $wmonth = date("m", $wt);
                $wyear = date("Y", $wt);
                $wdate = date('Y-n-d', $wt);

                $day_cell_text = utf8_strftime($dformat, $wt);
                $day_cell_link = "day.php?year=" . strftime("%Y", $wt) .
                    "&amp;month=" . strftime("%m", $wt) .
                    "&amp;day=" . strftime("%d", $wt) .
                    "&amp;area=$area";

                $tbody .= day_cell_html($day_cell_text, $day_cell_link, $wdate);
                for ($s = $morning_slot_seconds;
                     $s <= $evening_slot_seconds;
                     $s += $resolution)
                {
                    $is_invalid = $is_possibly_invalid[$thisday] && is_invalid_datetime(0, 0, $s, $wmonth, $wday, $wyear);
                    // set up the query strings to be used for the link in the cell
                    $query_strings = get_query_strings($user, $wmonth, $wday, $wyear, $s);

                    // and then draw the cell
                    if (!isset($week_map[0][$thisday][$s]))
                    {
                        $week_map[0][$thisday][$s] = array();  // to avoid an undefined index NOTICE error
                    }
                    $tbody .= cell_html($week_map[0][$thisday][$s], $query_strings, $is_invalid);
                }  // end looping through the time slots
                if ( FALSE != $row_labels_both_sides )
                {
                    $tbody .= day_cell_html($day_cell_text, $day_cell_link, $wdate);
                }
                $tbody .= "</tr>\n";
            }

        }  // end looping through the days of the week

    } // end "times along top" view (for the body)

    else
    {
        // the standard view, with days of the week along the top and times down the side
        for ($s = $morning_slot_seconds;
             $s <= $evening_slot_seconds;
             $s += $resolution,
             $row_class = ($row_class == "even_row") ? "odd_row" : "even_row")
        {
            // Show the time linked to the URL for highlighting that time:
            $class = $row_class;
            if (isset($timetohighlight) && ($s == $timetohighlight))
            {
                $class .= " row_highlight";
                $url = $base_url;
            }
            else
            {
                $url = $base_url . "&amp;timetohighlight=$s";
            }

            $tbody.= "<tr class=\"$class\">";
            $tbody .= time_cell_html($s, $url);

            // See note above: weekday==0 is day $weekstarts, not necessarily Sunday.
            for ($thisday = 0; $thisday<=($num_of_days-1) ; $thisday++)
            {
                if (is_hidden_day(($thisday + $weekstarts) % 7))
                {
                    // These days are to be hidden in the display
                    $tbody .= "<td class=\"hidden_day\">&nbsp;</td>\n";
                }
                else
                {
                    // set up the query strings to be used for the link in the cell
                    $wt = mktime(12, 0, 0, $month, $day_start_week+$thisday, $year);
                    $wday = date("d", $wt);
                    $wmonth = date("m", $wt);
                    $wyear = date("Y", $wt);
                    $is_invalid = $is_possibly_invalid[$thisday] && is_invalid_datetime(0, 0, $s, $wmonth, $wday, $wyear);
                    $query_strings = get_query_strings($user, $wmonth, $wday, $wyear, $s);
                    // and then draw the cell
                    if (!isset($week_map[0][$thisday][$s]))
                    {
                        $week_map[0][$thisday][$s] = array();  // to avoid an undefined index NOTICE error
                    }

                    $tbody .= cell_html($week_map[0][$thisday][$s], $query_strings, $is_invalid);
                }

            }    // for loop

            // next lines to display times on right side
            if ( FALSE != $row_labels_both_sides )
            {
                $tbody .= time_cell_html($s, $url);
            }

            $tbody .= "</tr>\n";
        }
    }  // end standard view (for the body)
    $tbody .= "</tbody>\n";
    return $thead . $tfoot . $tbody;
}

function map_add_booking ($row, &$column, $am7, $pm7)
{
    // Enters the contents of the booking found in $row into $column, which is
    // a column of the map of the bookings being prepared ready for display.
    //
    // $column    the column of the map that is being prepared (see below)
    // $row       a booking from the database
    // $am7       the start of the first slot of the booking day (Unix timestamp)
    // $pm7       the start of the last slot of the booking day (Unix timestamp)

    // $row is expected to have the following field names, when present:
    //       room_id
    //       start_time
    //       end_time
    //       name
    //       repeat_id
    //       entry_id
    //       type
    //       entry_description
    //       entry_create_by
    //       status

    // $column is a column of the map of the screen that will be displayed
    // It looks like:
    //     $column[s][n][id]
    //                  [is_repeat]
    //                  [is_multiday_start]  a boolean indicating if the booking stretches
    //                                       beyond the day start
    //                  [is_multiday_end]    a boolean indicating if the booking stretches
    //                                          beyond the day end
    //                  [color]
    //                  [data]
    //                  [long_descr]
    //                  [create_by]
    //                  [room_id]
    //                  [start_time]
    //                  [slots]
    //                  [status]

    // s is the number of nominal seconds (ie ignoring DST changes] since the
    // start of the calendar day which has the start of the booking day

    // slots records the duration of the booking in number of time slots.
    // Used to calculate how high to make the block used for clipping
    // overflow descriptions.

    // Fill in the map for this meeting. Start at the meeting start time,
    // or the day start time, whichever is later. End one slot before the
    // meeting end time (since the next slot is for meetings which start then),
    // or at the last slot in the day, whichever is earlier.
    // Time is of the format HHMM without leading zeros.
    //
    // [n] exists because it's possible that there may be multiple bookings
    // in the same time slot.   Normally this won't be the case.   However it
    // can arise legitimately if you increase the resolution, or shift the
    // displayed day.   For example if you previously had a resolution of 1800
    // seconds you might have a booking (A) for 1000-1130 and another (B) for 1130-1230.
    // If you then increase the resolution to 3600 seconds, these two bookings
    // will both occupy the 1100-1200 time slot.   [n] starts at 0.   For
    // the example above the map for the room would look like this
    //
    //       0  1
    // 1000  A
    // 1100  A  B
    // 1200  B
    //
    // Adjust the starting and ending times so that bookings which don't
    // start or end at a recognized time still appear.

    global $resolution;
    global $is_private_field;

    //$user = getUserName();
    /*if (is_private_event($row['status'] & STATUS_PRIVATE) &&
        !getWritable($row['entry_create_by'], $user, $row['room_id']))
    {
        $row['status'] |= STATUS_PRIVATE;   // Set the private bit
        if ($is_private_field['entry.name'])
        {
            $row['name']= "[".get_vocab('unavailable')."]";
        }
        if ($is_private_field['entry.description'])
        {
            $row['entry_description']= "[".get_vocab('unavailable')."]";
        }
    }
    else
    {
        $row['status'] &= ~STATUS_PRIVATE;  // Clear the private bit
    }*/

    $is_multiday_start = ($row['start_time'] < $am7);
    $is_multiday_end = ($row['end_time'] > ($pm7 + $resolution));

    $start_t = max(round_t_down($row['start_time'], $resolution, $am7), $am7);
    $end_t = min(round_t_up($row['end_time'], $resolution, $am7) - $resolution, $pm7);
    // calculate the times used for indexing - we index by nominal seconds since the start
    // of the calendar day which has the start of the booking day
    $start_s = nominal_seconds($start_t);
    $end_s = nominal_seconds($end_t);
    for ($s = $start_s; $s <= $end_s; $s += $resolution)
    {
        // find the first free index (in case there are multiple bookings in a timeslot)
        $n = 0;
        while (!empty($column[$s][$n]["id"]))
        {
            $n++;
        }
        // fill in the id, type and start time
        $column[$s][$n]["id"] = $row['entry_id'];
        $column[$s][$n]["is_repeat"] = isset($row['repeat_id']);
        $column[$s][$n]["is_multiday_start"] = $is_multiday_start;
        $column[$s][$n]["is_multiday_end"] = $is_multiday_end;
        $column[$s][$n]["status"] = $row['status'];
        $column[$s][$n]["color"] = $row['type'];
        $column[$s][$n]["start_time"] = hour_min($start_s);
        $column[$s][$n]["slots"] = NULL;  // to avoid undefined index NOTICE errors
        // if it's a multiple booking also fill in the name and description
        if ($n > 0)
        {
            $column[$s][$n]["data"] = $row['type'];
            //$column[$s][$n]["long_descr"] = $row['entry_description'];
            //$column[$s][$n]["create_by"] = $row['entry_create_by'];
            //$column[$s][$n]["room_id"] = $row['room_id'];
        }
        // otherwise just leave them blank (we'll fill in the first whole slot later)
        // use NULL rather than '' just in case the value really is ''
        else
        {
            $column[$s][$n]["data"] = NULL;
            $column[$s][$n]["long_descr"] = NULL;
            $column[$s][$n]["create_by"] = NULL;
            $column[$s][$n]["room_id"] = NULL;
        }
    } // end for


    // Show the name of the booker, the description and the number of complete
    // slots in the first complete slot that the booking happens in, or at the
    // start of the day if it started before today.

    // Find the number of time slots that the booking occupies, and the index
    // of the first slot that this booking has entirely to itself
    // We need to adjust the start and end times for DST transitions as the display
    // ignores DST
    $n_slots = intval((($end_t - $start_t) - cross_dst($start_t, $end_t))/$resolution) + 1;
    $first_slot = $start_s;

    // If the last time slot is already occupied, we have a multiple
    // booking in the last slot, so decrement the number of slots that
    // we will display for this booking
    if (isset($column[$end_s][1]["id"]))
    {
        $n_slots--;
        // If we're only the second booking to land on this time slot
        // then we'll have to adjust the information held for the first booking
        // (unless it's just one slot long in the first place, when it
        // doesn't matter as it will now be part of a multiple booking).
        // If we are the third booking or more, then it will have already
        // been adjusted.
        if (!isset($column[$end_s][2]["id"]))
        {
            if ($column[$end_s][0]["slots"] > 1)
            {
                // Move the name and description into the new first slot and decrement the number of slots
                $column[$end_s + $resolution][0]["data"]       = $column[$end_s][0]["data"];
                //$column[$end_s + $resolution][0]["long_descr"] = $column[$end_s][0]["long_descr"];
                //$column[$end_s + $resolution][0]["create_by"]  = $column[$end_s][0]["create_by"];
                //$column[$end_s + $resolution][0]["room_id"]    = $column[$end_s][0]["room_id"];
                $column[$end_s + $resolution][0]["slots"]      = $column[$end_s][0]["slots"] - 1;
            }
        }
    }

    // and if the first time slot is already occupied, decrement
    // again, adjust the first slot for this booking
    if (isset($column[$start_s][1]["id"]))
    {
        $n_slots--;
        $first_slot += $resolution;
        // If we're only the second booking to land on this time slot
        // then we'll have to adjust the information held for the first booking
        if (!isset($column[$start_s][2]["id"]))
        {
            // Find the first slot ($s) of the first booking
            $first_booking_id = $column[$start_s][0]["id"];
            $r = $start_s;
            // If you've got to the first slot of the day then that must be the
            // first slot of the first booking
            while ($r > $am7)
            {
                // Otherwise, step back one slot.
                $r -= $resolution;
                // If that slot contains the first booking, then step back again
                if (isset($column[$r]))
                {
                    foreach ($column[$r] as $booking)
                    {
                        if ($booking["id"] == $first_booking_id)
                        {
                            continue 2;  // next iteration of the while loop
                        }
                    }
                }
                // If not, then we've stepped back one slot past the start of
                // the first booking, so step forward again and finish
                $r += $resolution;
                break;
            } // end while

            // Now we've found the time ($r) of the first slot of the first booking
            // we need to find its index ($i)
            foreach ($column[$r] as $i => $booking)
            {
                if ($booking["id"] == $first_booking_id)
                {
                    break;
                }
            }

            // Finally decrement the slot count for the first booking
            // no need to worry about count going < 1: the multiple booking display
            // does not use the slot count.
            $column[$r][$i]["slots"]--;
            // and put the name and description in the multiply booked slot
            $column[$start_s][0]["data"]       = $column[$r][$i]["data"];
            //$column[$start_s][0]["long_descr"] = $column[$r][$i]["long_descr"];
            //$column[$start_s][0]["create_by"]  = $column[$r][$i]["create_by"];
            //$column[$start_s][0]["room_id"]    = $column[$r][$i]["room_id"];
        }
    }

    // now we've got all the information we can enter it in the first complete
    // slot for the booking (provided it's not a multiple booking slot)
    if (!isset($column[$first_slot][1]["id"]))
    {
        $column[$first_slot][0]["data"]       = $row['type'];
        //$column[$first_slot][0]["long_descr"] = $row['entry_description'];
        //$column[$first_slot][0]["create_by"]  = $row['entry_create_by'];
        //$column[$first_slot][0]["room_id"]    = $row['room_id'];
        $column[$first_slot][0]["slots"]      = $n_slots;
    }

} // end function map_add_booking()

function round_t_down($t, $resolution, $am7)
{
    return (int)$t - (int)abs(((int)$t-(int)$am7)
        % $resolution);
}

// Round time up to the nearest resolution
function round_t_up($t, $resolution, $am7)
{
    if (($t-$am7) % $resolution != 0)
    {
        return $t + $resolution - abs(((int)$t-(int)
                $am7) % $resolution);
    }
    else
    {
        return $t;
    }
}

function nominal_seconds($t)
{
    global $morningstarts, $morningstarts_minutes;

    $date = getdate($t);
    // check to see if the time is really on the next day
    if (hm_before($date,
        array('hours' => $morningstarts, 'minutes' => $morningstarts_minutes)))
    {
        $date['hours'] += 24;
    }
    return (($date['hours'] * 60) + $date['minutes']) * 60;
}

// Returms a string representing the hour and minute for the nominal
// seconds since the start of the day, $s
function hour_min($s)
{
    $following_day = ($s >= SECONDS_PER_DAY);
    $s = $s % SECONDS_PER_DAY;  // in case $s is on the next day
    // Choose a day that doesn't have any DST transitions in any timezone
    $t = mktime(0, 0, $s, 1, 1, 2000);
    $result = utf8_strftime(hour_min_format(), $t);
    if ($following_day)
    {
        $result = "* " . $result;
    }
    return $result;
}

function hour_min_format()
{
    global $twentyfourhour_format, $strftime_format;

    if ($twentyfourhour_format)
    {
        return $strftime_format['time24'];
    }
    else
    {
        return $strftime_format['time12'];
    }
}

// Returns a locale in the correct format for the server OS given a language
// Returns FALSE if no locale can be determined
function get_os_locale($lang)
{
    global $lang_map_unix, $lang_map_unix_postfix, $lang_map_windows;
    global $locale_warning, $server_os;

    $locale = FALSE;

    if ($server_os == "windows")
    {
        if (array_key_exists(utf8_strtolower($lang), $lang_map_windows))
        {
            $locale = $lang_map_windows[utf8_strtolower($lang)];
        }
        else
        {
            $locale_warning = "Server failed to map browser language '$lang' to a Windows locale specifier";
            trigger_error($locale_warning, E_USER_NOTICE);
        }
    }
    /* All of these Unix OSes work in mostly the same way... */
    else if (in_array($server_os, array('linux', 'sunos', 'bsd', 'aix', 'macosx')))
    {
        if (strlen($lang) == 2)
        {
            if (isset($lang_map_unix[$lang]) && ($lang_map_unix[$lang]))
            {
                $locale = $lang_map_unix[$lang];
            }
            else
            {
                // Convert locale=xx to xx_XX
                $locale = utf8_strtolower($lang) . "_" . utf8_strtoupper($lang);
            }
        }
        else
        {
            // First, if we have a map, use it
            if (isset($lang_map_unix[utf8_strtolower($lang)]))
            {
                $locale = $lang_map_unix[utf8_strtolower($lang)];
            }
            else
            {
                // Convert locale=xx-xX or xx_Xx or xx_XxXx (etc.) to xx_XX[XX]; this is highly
                // dependent on the machine's installed locales
                $locale = locale_format($lang, '_');
            }
        }
        switch ($server_os)
        {
            case "sunos":
            case "linux":
            case "bsd":
                $locale .= ".UTF-8";
                if (isset($lang_map_unix_postfix[$locale]))
                {
                    $locale .= $lang_map_unix_postfix[$locale];
                }
                break;

            case "macosx":
                $locale .= ".utf-8";
                break;
            default:
                break;
        }
    }
    return $locale;
}

// Works out what locale should be used by MRBS depending on the config settings
// and the user's browser preferences.
// Returns the locale, or FALSE if no locale can be determined
function determine_mrbs_locale()
{
    global $override_locale;
    global $locale, $windows_locale;

    if (isset($override_locale) && ($override_locale !== ''))
    {
        $mrbs_locale = $override_locale;
    }
    else
    {
        $mrbs_locale = get_os_locale($locale);
    }
    $windows_locale = $mrbs_locale;

    return $mrbs_locale;
}

// Wrapper round mb_strtolower() to make sure it's used if present and mb_string
// function overloading is not enabled.
// The ordinary strtolower() will give unexpected results when the locale is set to
// Turkish and will not convert the letter 'I'.
function utf8_strtolower($str)
{
    if (function_exists('mb_strtolower'))
    {
        return mb_strtolower($str);
    }
    return strtolower($str);
}

// Sets the locale according to the MRBS config settings and the user's
// browser preferences
function set_mrbs_locale()
{
    global $locale_warning;

    static $locale = '';
    static $have_locale = FALSE;
    static $have_valid_locale = FALSE;

    // If we've tried this before and have got a good locale, then set it.
    if ($have_locale)
    {
        if ($have_valid_locale)
        {
            setlocale(LC_ALL, $locale);
        }
        return;
    }
    // Otherwise work out what the locale should be and set it.
    $locale = determine_mrbs_locale();
    $have_locale = TRUE;
    if ($locale !== FALSE)
    {
        $have_valid_locale = (setlocale(LC_ALL, $locale) !== FALSE);
        if (!$have_valid_locale)
        {
            $locale_warning = "Server failed to set locale to '$locale'";
        }
    }
}

//  optionally switch to a new locale, switching back at the end
function utf8_strftime($format, $time, $temp_locale=NULL)
{
    global $server_os;
    // Set the temporary locale
    if (!empty($temp_locale))
    {
        $old_locale = setlocale(LC_TIME, '0');
        setlocale(LC_TIME, $temp_locale);
    }
    elseif ($server_os == "windows")
    {
        // If we are running Windows we have to set the locale again in case another script
        // running in the same process has changed the locale since we first set it.  See the
        // warning on the PHP manual page for setlocale():
        //
        // "The locale information is maintained per process, not per thread. If you are
        // running PHP on a multithreaded server API like IIS or Apache on Windows, you may
        // experience sudden changes in locale settings while a script is running, though
        // the script itself never called setlocale(). This happens due to other scripts
        // running in different threads of the same process at the same time, changing the
        // process-wide locale using setlocale()."
        set_mrbs_locale();
    }
    if ($server_os == "windows")
    {
        // Some formats not supported on Windows.   Replace with suitable alternatives
        $format = str_replace("%R", "%H:%M", $format);
        $format = str_replace("%P", "%p", $format);
        $format = str_replace("%l", "%I", $format);
        $format = str_replace("%e", "%#d", $format);
    }

    // %p doesn't actually work in some locales, we have to patch it up ourselves
    if (preg_match('/%p/', $format))
    {
        $ampm = strftime('%p', $time);  // Don't replace the %p with the $strftime_format variable!!
        if ($ampm == '')
        {
            $ampm = date('a', $time);
        }

        $format = preg_replace('/%p/', $ampm, $format);
    }
    $result = strftime($format, $time);
    $result = utf8_convert_from_locale($result, $temp_locale);
    // Restore the original locale
    if (!empty($temp_locale))
    {
        setlocale(LC_TIME, $old_locale);
    }

    return $result;
}

function utf8_convert_from_locale($string, $locale=NULL)
{
    global $windows_locale, $winlocale_codepage_map, $server_os;

    if ($server_os == "windows")
    {
        if (!isset($locale))
        {
            $locale = $windows_locale;
        }
        if (array_key_exists($locale, $winlocale_codepage_map))
        {
            $string = iconv($winlocale_codepage_map[$locale],"utf-8",
                $string);
        }
    }
    else if ($server_os == "aix")
    {
        $string = utf8_convert_aix($string, $locale);
    }
    return $string;
}

// Returns TRUE if the end of the last slot is on the day after the beginning
// of the first slot
function day_past_midnight()
{
    global $morningstarts, $morningstarts_minutes, $eveningends, $eveningends_minutes, $resolution;

    $start_first_slot = (($morningstarts * 60) + $morningstarts_minutes) * 60;
    $end_last_slot = ((($eveningends * 60) + $eveningends_minutes) * 60) + $resolution;
    $end_last_slot = $end_last_slot % SECONDS_PER_DAY;

    return ($end_last_slot <= $start_first_slot);
}

// Gets the number of time slots between the beginning and end of the booking
// day.   (This is the normal number on a non-DST transition day)
function get_n_time_slots()
{
    global $morningstarts, $morningstarts_minutes, $eveningends, $eveningends_minutes;
    global $resolution;

    $start_first = (($morningstarts * 60) + $morningstarts_minutes) * 60;           // seconds
    $end_last = ((($eveningends * 60) + $eveningends_minutes) * 60) + $resolution;  // seconds
    $end_last = $end_last % SECONDS_PER_DAY;
    if (day_past_midnight())
    {
        $end_last += SECONDS_PER_DAY;
    }
    $n_slots = ($end_last - $start_first)/$resolution;

    return $n_slots;
}

function get_main_column_width($n_columns, $n_hidden=0)
{
    global $row_labels_both_sides, $first_last_width, $column_hidden_width;

    // Calculate the percentage width of each of the main columns.   We use number_format
    // because in some locales you would get a comma for the decimal point which
    // would break the CSS
    $column_width = 100 - $first_last_width;
    if (!empty($row_labels_both_sides))
    {
        $column_width = $column_width - $first_last_width;
    }
    // Subtract the hidden columns (unless they are all hidden)
    if ($n_hidden < $n_columns)
    {
        $column_width = $column_width - ($n_hidden * $column_hidden_width);
        $column_width = number_format($column_width/($n_columns - $n_hidden), 6);
    }

    return $column_width;
}

function is_hidden_day ($dow)
{
    global $hidden_days;
    return (isset($hidden_days) && in_array($dow, $hidden_days));
}

// Draw a day cell to be used in the first and last columns of the week view
//    $text     contains the date, formatted as a string
//    $link     the href to be used for the link
//    $date     the date in yyyy-mm-dd format
function day_cell_html($text, $link, $date)
{
    $html = '';
    $html .= tdcell("row_labels", 1, array('name' => 'date', 'value' => $date));
    $html .= "<div class=\"celldiv slots1\">\n";
    $html .= "<a href=\"$link\" title=\"" . get_vocab("viewday") . "\">$text</a>\n";
    $html .= "</div></td>\n";
    return $html;
}

// Checks whether the nominal time given is an invalid date and time with respect to
// DST transitions.   When entering DST there is a set of times that don't exist, for
// example from 0100 to 0159 in Europe/London.
// Returns NULL if MRBS is unable to determine an answer, otherwise TRUE or FALSE (so
// a simple equality test will default to a valid time if MRBS can't determine an answer)
function is_invalid_datetime($hour, $minute, $second, $month, $day, $year, $tz=NULL)
{
    global $timezone;

    // Do a quick check to see if there's a possibility of an invalid time by checking
    // whether there's a transition into DST from the day before to the day after
    if (!function_exists('date_default_timezone_set'))
    {
        return NULL;
    }

    if (empty($tz))
    {
        $tz = $timezone;  // default to the current timezone
    }

    $old_tz = date_default_timezone_get();  // save the current timezone
    date_default_timezone_set($tz);

    // If the day before is in DST then the datetime must be valid, because
    // you only get the gap when entering DST.
    $dayBefore = mktime($hour, $minute, $second, $month, $day-1, $year);
    if (date('I', $dayBefore))
    {
        $result = FALSE;
    }
    else
    {
        // The day before is not in DST.   If the day after is also not in DST,
        // then there can have been no transition, so again the datetime must be valid.
        $dayAfter = mktime($hour, $minute, $second, $month, $day+1, $year);
        if (!date('I', $dayAfter))
        {
            $result = FALSE;
        }
        else
        {
            // We are in a transition into DST, so we need to check more carefully.
            // However we can only do this efficiently in PHP 5.3.0 or greater
            if (version_compare(PHP_VERSION, '5.3.0') < 0)
            {
                $result = NULL;
            }
            else
            {
                $thisDateTimeZone = new DateTimeZone($tz);
                // Get the transition data (we assume there is one and only one transition),
                // in particular the time at which the transition happens and the new offset
                $transitions = $thisDateTimeZone->getTransitions($dayBefore, $dayAfter);
                // According to my reading of the PHP manual, getTransitions() should return
                // all transitions between the start and end date.   However what it seems to do
                // is return an array consisting of the time data for the start date followed by
                // the transition data.   So as a precaution we take the last element of the array
                // (we were only expecting one element, but seem to get two).
                $transition = array_pop($transitions);
                // If we failed for some reason to get any transition data, return NULL
                if (!isset($transition))
                {
                    $result = NULL;
                }
                else
                {
                    // Get the old offset and work out how many seconds the clocks change by
                    $beforeDateTime = new DateTime(date('c', $dayBefore), $thisDateTimeZone);
                    $change = $transition['offset'] - $beforeDateTime->getOffset();

                    // See if the nominal date falls outside the gap
                    $lastValidSecond = getdate($transition['ts'] - 1);
                    $lastInvalidSecond = $lastValidSecond;
                    $lastInvalidSecond['seconds'] += $change;
                    $thisDate = array('hours' => $hour, 'minutes' => $minute, 'seconds' => $second,
                        'mon' => $month, 'mday' => $day, 'year' => $year);

                    $result = ((nominal_date_compare($thisDate, $lastValidSecond) > 0) &&
                        (nominal_date_compare($thisDate, $lastInvalidSecond) <= 0));
                }
            }
        }
    }

    date_default_timezone_set($old_tz);  // restore the old timezone

    return $result;
}

// $s is nominal seconds
function get_query_strings($user, $month, $day, $year, $s)
{
    global $morningstarts, $morningstarts_minutes;

    $query_strings = array();

    // check to see if the time is really on the next day
    $date = getdate(mktime(0, 0, $s, $month, $day, $year));
    if (hm_before($date,
        array('hours' => $morningstarts, 'minutes' => $morningstarts_minutes)))
    {
        $date['hours'] += 24;
    }
    $hour = $date['hours'];
    $minute = $date['minutes'];
    //$period = period_index($s);
    $query_strings['new_periods'] = "user=$user&amp;period=$period&amp;year=$year&amp;month=$month&amp;day=$day";
    $query_strings['new_times']   = "user=$user&amp;hour=$hour&amp;minute=$minute&amp;year=$year&amp;month=$month&amp;day=$day";
    $query_strings['booking']     = "user=$user&amp;day=$day&amp;month=$month&amp;year=$year";

    return $query_strings;
}

function cell_html($cell, $query_strings, $is_invalid = FALSE)
{
    // draws a single cell in the main table of the day and week views
    //
    // $cell is a two dimensional array that is part of the map of the whole
    // display and looks like this:
    //
    // $cell[n][id]
    //         [is_repeat]
    //         [is_multiday_start]
    //         [is_multiday_end]
    //         [color]
    //         [data]
    //         [long_descr]
    //         [create_by]
    //         [room_id]
    //         [start_time]
    //         [slots]
    //         [status]
    //
    // where n is the number of the booking in the cell.    There can be none, one or many
    // bookings in a cell.    If there are no bookings then a blank cell is drawn with a link
    // to the edit entry form.     If there is one booking, then the booking is shown in that
    // cell.    If there is more than one booking then all the bookings are shown, but they can
    // be shown in two different ways: minimised and maximised.   By default they are shown
    // minimised, so that the standard row height is preserved.   By clicking a control
    // the cell can be maximised.   (Multiple bookings can arise in a cell if the resolution
    // of an existing database in increased or the booking day is shifted).

    // $query_strings is an array containg the query strings (or partial query strings) to be
    // appended to the link used for the cell.    It is indexed as follows:
    //    ['new_periods']   the string to be used for an empty cell if using periods
    //    ['new_times']     the string to be used for an empty cell if using times
    //    ['booking']       the string to be used for a full cell
    //
    // $is_invalid specifies whether the slot actually exists or is one of the non-existent
    // slots in the transition to DST

    global $main_cell_height, $main_table_cell_border_width;
    global $user, $month, $timetohighlight;
    global $enable_periods, $times_along_top, $show_plus_link;
    global $approval_enabled, $confirmation_enabled;

    $html = '';
    // if the time slot has got multiple bookings, then draw a mini-table
    if(isset($cell[1]["id"]))
    {
        // Find out how many bookings there are (needed to calculate heights)
        $n_bookings = 0;
        while (isset($cell[$n_bookings]["id"]))
        {
            $n_bookings++;
        }

        // Make the class maximized by default so that if you don't have JavaScript then
        // you can still see all the bookings.    If you have JavaScript it will overwrite
        // the class and make it minimized.
        $html .= "<td class=\"multiple_booking maximized\">\n";

        // First draw the mini table
        $html .= "<div class=\"celldiv slots1 mini\">\n";
        $html .= "<div class=\"multiple_control\">+</div>\n";
        $html .= "<table>\n";
        $html .= "<tbody>\n";

        $row_height = $main_cell_height - (($n_bookings-1) * $main_table_cell_border_width);   // subtract the borders (first row has no top border)
        $row_height = $row_height/$n_bookings;  // split what's left between the bookings
        $row_height = (int) ceil($row_height);  // round up, so that (a) there's no empty space at the bottom
        // and (b) each stripe is at least 1 unit high
        for ($n=0; $n<$n_bookings; $n++)
        {
            $id          = $cell[$n]["id"];
            $is_repeat   = $cell[$n]["is_repeat"];
            $is_multiday_start = $cell[$n]["is_multiday_start"];
            $is_multiday_end   = $cell[$n]["is_multiday_end"];
            $status      = $cell[$n]["status"];
            $color       = $cell[$n]["color"];
            $descr       = htmlspecialchars($cell[$n]["data"]);
            $long_descr  = htmlspecialchars($cell[$n]["long_descr"]);
            $class = $color;
            if ($status & STATUS_PRIVATE)
            {
                $class .= " private";
            }
            if ($approval_enabled && ($status & STATUS_AWAITING_APPROVAL))
            {
                $class .= " awaiting_approval";
            }
            if ($confirmation_enabled && ($status & STATUS_TENTATIVE))
            {
                $class .= " tentative";
            }
            if ($is_multiday_start)
            {
                $class .= " multiday_start";
            }
            if ($is_multiday_end)
            {
                $class .= " multiday_end";
            }

            $html .= "<tr>\n";
            $html .= "<td class=\"$class\"" .
                (($n==0) ? " style=\"border-top-width: 0\"" : "") .   // no border for first row
                ">\n";
            $html .= "<div style=\"overflow: hidden; " .
                "height: " . $row_height . "px; " .
                "max-height: " . $row_height . "px; " .
                "min-height: " . $row_height . "px\">\n";
            $html .= "&nbsp;\n";
            $html .= "</div>\n";
            $html .= "</td>\n";
            $html .= "</tr>\n";
        }
        $html .= "</tbody>\n";
        $html .= "</table>\n";
        $html .= "</div>\n";

        // Now draw the maxi table
        $html .= "<div class=\"maxi\">\n";
        $total_height = $n_bookings * $main_cell_height;
        $total_height += ($n_bookings - 1) * $main_table_cell_border_width;  // (first row has no top border)
        $html .= "<div class=\"multiple_control\" " .
            "style =\"height: " . $total_height . "px; " .
            "min-height: " . $total_height . "px; " .
            "max-height: " . $total_height . "px; " .
            "\">-</div>\n";
        $html .= "<table>\n";
        $html .= "<tbody>\n";
        for ($n=0; $n<$n_bookings; $n++)
        {
            $id          = $cell[$n]["id"];
            $is_repeat   = $cell[$n]["is_repeat"];
            $is_multiday_start = $cell[$n]["is_multiday_start"];
            $is_multiday_end   = $cell[$n]["is_multiday_end"];
            $status      = $cell[$n]["status"];
            $color       = $cell[$n]["color"];
            $descr       = htmlspecialchars($cell[$n]["start_time"] . " " . $cell[$n]["data"]);
            $long_descr  = htmlspecialchars($cell[$n]["long_descr"]);
            $class = $color;
            if ($status & STATUS_PRIVATE)
            {
                $class .= " private";
            }
            if ($approval_enabled && ($status & STATUS_AWAITING_APPROVAL))
            {
                $class .= " awaiting_approval";
            }
            if ($confirmation_enabled && ($status & STATUS_TENTATIVE))
            {
                $class .= " tentative";
            }
            if ($is_multiday_start)
            {
                $class .= " multiday_start";
            }
            if ($is_multiday_end)
            {
                $class .= " multiday_end";
            }
            $html .= "<tr>\n";
            $html .= "<td class=\"$class\"" .
                (($n==0) ? " style=\"border-top-width: 0\"" : "") .   // no border for first row
                ">\n";
            $html .= "<div class=\"celldiv slots1\">\n";     // we want clipping of overflow
            $html .= "<a href=\"view_entry.php?id=$id&amp;". $query_strings['booking'] . "\" title=\"$long_descr\">";
            $html .= ($is_repeat) ? "<img class=\"repeat_symbol\" src=\"images/repeat.png\" alt=\"" . get_vocab("series") . "\" title=\"" . get_vocab("series") . "\" width=\"10\" height=\"10\">" : '';
            $html .= "$descr</a>\n";
            $html .= "</div>\n";

            $html .= "</td>\n";
            $html .= "</tr>\n";
        }
        $html .= "</tbody>\n";
        $html .= "</table>\n";
        $html .= "</div>\n";

        $html .= "</td>\n";
    }  // end of if isset ( ...[1]..)

    // otherwise draw a cell, showing either the booking or a blank cell
    else
    {

        if(isset($cell[0]["id"]))
        {
            $id         = $cell[0]["id"];
            $is_repeat  = $cell[0]["is_repeat"];
            $is_multiday_start = $cell[0]["is_multiday_start"];
            $is_multiday_end   = $cell[0]["is_multiday_end"];
            $status     = $cell[0]["status"];
            $color      = $cell[0]["color"];
            $descr      = isset($cell[0]["data"]) ? htmlspecialchars($cell[0]["data"]) : NULL;
            $long_descr = htmlspecialchars($cell[0]["long_descr"]);
            $slots      = $cell[0]["slots"];
        }
        else  // id not set
        {
            unset($id);
            $slots = 1;
        }

        // $c is the colour of the cell that the browser sees. Zebra stripes normally,
        // row_highlight if we're highlighting that line and the appropriate colour if
        // it is booked (determined by the type).
        // We tell if its booked by $id having something in it
        if (isset($id))
        {
            $c = $color;
            if ($status & STATUS_PRIVATE)
            {
                $c .= " private";
            }
            if ($approval_enabled && ($status & STATUS_AWAITING_APPROVAL))
            {
                $c .= " awaiting_approval";
            }
            if ($confirmation_enabled && ($status & STATUS_TENTATIVE))
            {
                $c .= " tentative";
            }
            if ($is_multiday_start)
            {
                $c .= " multiday_start";
            }
            if ($is_multiday_end)
            {
                $c .= " multiday_end";
            }
            // Add a class to bookings that this user is allowed to edit so that the
            // JavaScript can turn them into resizable bookings
            if (getWritable($cell[0]['create_by'], $user, $cell[0]['room_id']))
            {
                $c .= " writable";
                if ($is_repeat)
                {
                    $c .= " series";
                }
            }
        }
        else
        {
            $c = ($is_invalid) ? "invalid" : "new";
        }

        // Don't put in a <td> cell if the slot is booked and there's no description.
        // This would mean that it's the second or subsequent slot of a booking and so the
        // <td> for the first slot would have had a rowspan that extended the cell down for
        // the number of slots of the booking.

        if (!isset($id) || isset($descr))
        {
            $html .= tdcell($c, $slots);

            // If the room isn't booked then allow it to be booked
            if (!isset($id))
            {
                // Don't provide a link if the slot doesn't really exist
                if (!$is_invalid)
                {
                    $html .= "<div class=\"celldiv slots1\">\n";  // a bookable slot is only one unit high
                    $html .= "<a href=\"edit_entry.php?" .
                        (($enable_periods) ? $query_strings['new_periods'] : $query_strings['new_times']) .
                        "\">\n";
                    if ($show_plus_link)
                    {
                        $html .= "<img src=\"images/new.gif\" alt=\"New\" width=\"10\" height=\"10\">\n";
                    }
                    $html .= "</a>\n";
                    $html .= "</div>\n";
                }
            }
            else                 // if it is booked then show the booking
            {
                $html .= "<div data-id=\"$id\" class=\"celldiv slots" .
                    (($times_along_top) ? "1" : $slots) .
                    "\">\n";
                $html .= "<a href=\"view_entry.php?id=$id&amp;". $query_strings['booking'] . "\" title=\"$long_descr\">";
                $html .= ($is_repeat) ? "<img class=\"repeat_symbol $c\" src=\"images/repeat.png\" alt=\"" . get_vocab("series") . "\" title=\"" . get_vocab("series") . "\" width=\"10\" height=\"10\">" : '';
                $html .= "$descr</a>\n";
                $html .= "</div>\n";
            }
            $html .= "</td>\n";
        }
    }

    return $html;
}  // end function draw_cell

// Draw a time cell to be used in the first and last columns of the day and week views
//    $s                 the number of seconds since the start of the day (nominal - not adjusted for DST)
//    $url               the url to form the basis of the link in the time cell
function time_cell_html($s, $url)
{
    global $enable_periods, $periods;

    $html = '';

    $html .= tdcell("row_labels", 1, array('name' => 'seconds', 'value' => $s));
    $html .= "<div class=\"celldiv slots1\">\n";
    if ( $enable_periods )
    {

        $html .= "<a href=\"$url\"  title=\""
            . get_vocab("highlight_line") . "\">"
            . period_name($s) . "</a>\n";
    }
    else
    {
        $html .= "<a href=\"$url\" title=\""
            . get_vocab("highlight_line") . "\">"
            . hour_min($s) . "</a>\n";
    }
    $html .= "</div></td>\n";

    return $html;
}

// Output a start table cell tag <td> with color class.
// $colclass is an entry type (A-J), zebra stripes if
// empty or row_highlight if highlighted.
// $slots is the number of time slots high that the cell should be
//
// $data is an optional third parameter which if set passes the data name and value
// for use in the data- attribute.  It is an associative array with two elements,
// data['name'] and data['value'].
function tdcell($colclass, $slots)
{
    global $times_along_top;

    $html = '';
    if (func_num_args() > 2)
    {
        $data = func_get_arg(2);
    }

    $html .= "<td class=\"$colclass\"";
    if ($slots > 1)
        // No need to output more HTML than necessary
    {
        $html .= " " . (($times_along_top) ? "colspan" : "rowspan") . "=\"$slots\"";
    }
    if (isset($data))
    {
        $html .= " data-" . $data['name'] . "=\"" . $data['value'] . "\"";
    }
    $html .= ">\n";
    return $html;
}

/* getWritable($creator, $user, $room)
 *
 * Determines if a user is able to modify an entry
 *
 * $creator - The creator of the entry
 * $user    - Who wants to modify it
 * $room    - The id of the room that the entry is in
 *
 * Returns:
 *   0        - The user does not have the required access
 *   non-zero - The user has the required access
 */
function getWritable($creator, $user, $room)
{
    // Always allowed to modify your own stuff
    if(strcasecmp($creator, $user) == 0)
    {
        return 1;
    }

/*    // Otherwise you have to be a (booking) admin for this room
    if (auth_book_admin($user, $room))
    {
        return 1;
    }*/

    // Unathorised access
    return 1;
}

// Print the page header
function print_header($day, $month, $year, $user)
{
    global $theme, $done_header;

    if ($done_header)
    {
        return;
    }

    // Load the print_theme_header function appropriate to the theme.    If there
    // isn't one then fall back to the default header.
    if (file_exists("Themes/$theme/header.inc"))
    {
        include_once "Themes/$theme/header.inc";
    }
    if (!function_exists("print_theme_header"))
    {
        require_once "Themes/default/header.inc";
    }
    // Now go and do it
    print_theme_header($day, $month, $year, $user);
    $done_header = TRUE;
}

function get_charset()
{
    return 'utf-8';
}

// Outputs the HTTP headers
function http_headers()
{
    header("Content-Type: text/html; charset=" . get_charset());
    // We want to prevent IE8 offering "Compatability View" as that really
    // messes up MRBS, especially on the Report page
    header("X-UA-Compatible: IE=Edge");
    header("Pragma: no-cache");                          // HTTP 1.0
    header("Expires: Mon, 26 Jul 1997 05:00:00 GMT");    // Date in the pastt
}

// Tests whether an array is associative
//
// Thanks to magentix at gmail dot com at http://php.net/manual/function.is-array.php
function is_assoc($arr)
{
    return (is_array($arr) && count(array_filter(array_keys($arr),'is_string')) == count($arr));
}

// Generates a select box with an associated label
//
//   $params    an associative array holding the function parameters:
//      MANDATORY
//        'name'          The name of the element.
//      OPTIONAL
//        'id'            The id of the element.  Defaults to the same as the name.
//        'label'         The text to be used for the field label.
//        'label_title'   The text to be used for the title attribute for the field label
//        'options'       An array of options for the select element.   Can be a simple
//                        array or an associative array with value => text members for
//                        each <option> in the <select> element.   Default is an empty array.
//        'force_assoc'   Boolean.  Forces the options array to be treated as an
//                        associative array.  Default FALSE, ie it is treated as whatever
//                        it looks like.  (This parameter is necessary because if you
//                        index an array with strings that look like integers then PHP
//                        casts the keys to integers and the array becomes a simple array)
//        'value'         The value of the input.  Default ''.   Can be a single value
//                        or an array of values.
//        'size'          The value of the 'size' attribute.  Default NULL
//        'disabled'      Whether the field should be disabled.  Default FALSE
//        'create_hidden' Boolean.  If TRUE hidden inputs are created if 'disabled' is set
//                        Default TRUE
//        'mandatory'     Whether the field is a required field.  Default FALSE
//        'multiple'      Whether multiple selections are allowed.  Default FALSE
//        'attributes'    Additional attributes not covered explicitly above.  Default NULL.
//                        Can be either a simple string or an array of attributes.
//
function generate_select($params)
{
    // some sanity checking on params
    foreach (array('label', 'label_title', 'name', 'id', 'options', 'force_assoc',
                 'value', 'size', 'disabled', 'create_hidden', 'mandatory',
                 'multiple', 'attributes') as $key)
    {
        if (!isset($params[$key]))
        {
            switch ($key)
            {
                case 'name':
                    trigger_error('Missing mandatory parameters', E_USER_NOTICE);
                    break;
                case 'id':
                    $params[$key] = $params['name'];
                    break;
                case 'label_title':
                    $params[$key] = '';
                    break;
                case 'options':
                    $params[$key] = array();
                    break;
                case 'value':
                    $params[$key] = array();
                    break;
                case 'force_assoc':
                case 'disabled':
                case 'mandatory':
                case 'multiple':
                    $params[$key] = FALSE;
                    break;
                case 'create_hidden':
                    $params[$key] = TRUE;
                    break;
                default:
                    break;
            }
        }
    }

    // Some checking that we're obeying the HTML5 rules.   A bit pedantic, but
    // helps with debugging.  Only carry out these checks if we're going to be
    // able to report them.
    if (error_reporting() & E_USER_NOTICE)
    {
        if ((!isset($params['size']) || ($params['size'] == 1)) &&
            $params['mandatory'] &&
            !$params['multiple'])
        {
            if (count($params['options']) == 0)
            {
                $message =  "A select element with a required attribute and without a multiple " .
                    "attribute, and whose size is 1, must have a child option element.";
                trigger_error($message, E_USER_NOTICE);
            }
            else
            {
                $first_child = each($params['options']);
                if (($first_child['key'] !== '') && ($first_child['value'] !== ''))
                {
                    $message = "The first child option element of a select element with a required " .
                        "attribute and without a multiple attribute, and whose size is 1, " .
                        "must have either an empty value attribute, or must have no text content.";
                    trigger_error($message, E_USER_NOTICE);
                }
            }
        }
    }

    if (!is_array($params['value']))
    {
        $params['value'] = array($params['value']);
    }

    if (isset($params['attributes']) && is_array($params['attributes']))
    {
        $params['attributes'] = implode(' ', $params['attributes']);
    }

    // generate the HTML
    // no HTML escaping for the label - it is trusted

    $html = '';
    if (isset($params['label']))
    {
        $html .= "<label for=\"" .$params['id'] . "\"";
        $html .= (empty($params['label_title'])) ? '' : ' title="' . htmlspecialchars($params['label_title']) . '"';
        $html .= ">" . $params['label'] . "</label>\n";
    }
    $html .= "<select id=\"" . $params['id'] . "\" name=\"" . $params['name'] . "\"";
    $html .= ($params['disabled']) ? " disabled=\"disabled\"" : "";
    $html .= ($params['mandatory']) ? " required aria-required=\"true\"" : "";
    $html .= ($params['multiple']) ? " multiple" : "";
    $html .= (isset($params['attributes'])) ? " " . $params['attributes'] : "";
    $html .= ">\n";

    foreach ($params['options'] as $value => $text)
    {
        // We can cope with both associative and ordinary arrays
        if (!$params['force_assoc'] && !is_assoc($params['options']))
        {
            $value = $text;
        }
        $html .= "<option value=\"" . htmlspecialchars($value) . "\"";
        $html .= (in_array($value, $params['value'])) ? " selected=\"selected\"" : '';
        $html .= ">".htmlspecialchars($text)."</option>\n";
    }

    $html .= "</select>\n";

    // and hidden inputs if the select box is disabled
    if ($params['disabled'] && $params['create_hidden'])
    {
        foreach ($params['value'] as $value)
        {
            $html .= "<input type=\"hidden\" name=\"" . $params['name'] . "\" value=\"".
                htmlspecialchars($value)."\">\n";
        }
    }
    echo $html;
}

// Generates a date selector for use on a form.   If JavaScript is enabled
// then it will generate a calendar picker using jQuery UI datepicker.   If not,
// it will generate three separate select boxes, one each for day, month and year.
//
// $form_id is an optional fifth parameter.   If set it specifies the id of
// a form to submit when the datepicker is closed.
//
// $disabled:  TRUE if the SELECT is to be disabled (Note: hidden inputs will be
// created if the SELECT is disabled)
//
// Whether or not JavaScript is enabled the date is passed back in three separate
// variables:  ${prefix}day, ${prefix}month and ${prefix}year
//
// The function passes back three separate variables, rather than a single date
// variable, partly for compatibility with previous implementations of genDateSelector()
// and partly because it's easier to do this for the non-JavaScript version.
function genDateSelector($prefix, $day, $month, $year, $form_id='', $disabled=FALSE)
{
    global $strftime_format, $year_range;

    // Make sure we've got a date
    if (empty($day) or empty($month) or empty($year))
    {
        $day   = date("d");
        $month = date("m");
        $year  = date("Y");
    }

    // Cast the dates to ints to remove any leading zeroes (otherwise
    // JavaScript will treat them as octal numbers)
    $day = (int) $day;
    $month = (int) $month;
    $year = (int) $year;

    // First and last dates to show in year select
    $min = min($year, date("Y")) - $year_range['back'];
    $max = max($year, date("Y")) + $year_range['ahead'];

    // We'll put the date selector in a span.    First of all we'll generate
    // day, month and year selectors.   These will be used if JavaScript is
    // disabled.    If JavaScript is enabled it will overwrite these with a
    // datepicker calendar.
    echo "<span class=\"dateselector js_hidden\"" .
        " data-prefix=\"$prefix\"" .
        " data-day=\"$day\"" .
        " data-month=\"$month\"" .
        " data-year=\"$year\"" .
        " data-min-year=\"$min\"" .
        " data-max-year=\"$max\"" .
        " data-form-id=\"$form_id\">\n";

    // the day selector
    $options = array();
    for ($i = 1; $i <= 31; $i++)
    {
        $options[] = $i;
    }
    $params = array('name'     => $prefix . 'day',
        'options'  => $options,
        'value'    => $day,
        'disabled' => $disabled);
    generate_select($params);

    // the month selector
    $options = array();
    for ($i = 1; $i <= 12; $i++)
    {
        $options[$i] = utf8_strftime($strftime_format['mon'], mktime(0, 0, 0, $i, 1, $year));
    }
    $params = array('name'        => $prefix . 'month',
        'options'     => $options,
        'force_assoc' => TRUE,
        'value'       => $month,
        'disabled'    => $disabled);
    generate_select($params);

    // the year selector
    $options = array();
    for ($i = $min; $i <= $max; $i++)
    {
        $options[] = $i;
    }
    $params = array('name'     => $prefix . 'year',
        'options'  => $options,
        'value'    => $year,
        'disabled' => $disabled);
    generate_select($params);

    echo "</span>\n";
}

function make_area_select_html($link, $current, $year, $month, $day)
{
    global $area_list_format;
    $area_list_format = 'select';
    $out_html = '';

    $users = get_areas();
    // Only show the areas if there are more than one of them, otherwise
    // there's no point
    if (count($users) > 1)
    {
        $out_html .= "<div id=\"dwm_areas\">\n";
        $out_html .= "<h3>" . get_vocab("areas") . "</h3>\n";
        if ($area_list_format == "select")
        {
            $out_html .= "<form id=\"areaChangeForm\" method=\"get\" action=\"$link\">\n" .
                "<div>\n" .
                "<select class=\"room_area_select\" id=\"area_select\" name=\"user\" onchange=\"this.form.submit()\">";
            foreach ($users as $code => $name)
            {
                $selected = ($code == $current) ? "selected=\"selected\"" : "";
                $out_html .= "<option $selected value=\"". $code . "\">" . htmlspecialchars($name) . "</option>\n";
            }
            // Note:  the submit button will not be displayed if JavaScript is enabled
            $out_html .= "</select>\n" .
                "<input type=\"hidden\" name=\"day\"   value=\"$day\">\n" .
                "<input type=\"hidden\" name=\"month\" value=\"$month\">\n" .
                "<input type=\"hidden\" name=\"year\"  value=\"$year\">\n" .
                //"<input type=\"submit\" class=\"js_none\" value=\"".get_vocab("change")."\">\n" .
                "</div>\n" .
                "</form>\n";
        }
        else // list format
        {
            $out_html .= "<ul>\n";
            foreach ($areas as $area_id => $area_name)
            {
                $out_html .= "<li><a href=\"$link?year=$year&amp;month=$month&amp;day=$day&amp;area=${area_id}\">";
                $out_html .= "<span" . (($area_id == $current) ? ' class="current"' : '') . ">";
                $out_html .= htmlspecialchars($area_name) . "</span></a></li>\n";
            }
            $out_html .= "</ul>\n";
        }
        $out_html .= "</div>\n";
    }
    return $out_html;
} // end make_area_select_html

function get_areas($all=FALSE)
{
    global $tbl_area;

    $users = array();

    $sql = "SELECT code, name FROM users";
    if (empty($all))
    {
        $sql .= " WHERE disabled=0";
    }
    $sql .= " ORDER BY name";
    $res = sql_query($sql);
    if ($res === FALSE)
    {
        trigger_error(sql_error(), E_USER_WARNING);
    }
    else
    {
        for ($i=0; $row = sql_mysqli_row_keyed($res, $i); $i++)
        {
            $users[$row['code']] = $row['name'];
        }
    }

    return $users;
}

function print_footer($and_exit)
{
    global $theme;
    if (file_exists("Themes/$theme/footer.inc"))
    {
        include_once "Themes/$theme/footer.inc";
    }
    if (function_exists("print_theme_footer"))
    {
        print_theme_footer();
    }
    else {
        ?>
        </body>
        </html>
        <?php
    }
    if ($and_exit)
    {
        exit(0);
    }
}

function date_byday($timestamp)
{
    global $RFC_5545_days;

    $dow = $RFC_5545_days[date('w', $timestamp)];
    $dom = date('j', $timestamp);
    $ord = intval(($dom - 1)/7) + 1;
    if ($ord == 5)
    {
        $ord = -1;
    }
    return $ord . $dow;
}

// Splits a BYDAY string into its ordinal and day parts, returned as a simple array.
// For example "-1SU" is returned as array("-1", "SU");
function byday_split($byday)
{
    $result = array();
    $split_pos = strlen($byday) -2;
    $result[] = substr($byday, 0, $split_pos);
    $result[] = substr($byday, $split_pos, 2);
    return $result;
}

// Generate a checkbox with an associated label
//
//   $params    an associative array holding the function parameters:
//      MANDATORY
//        'label'         The text to be used for the field label.
//        'name'          The name of the element.
//      OPTIONAL
//        'label_after'   Whether to put the label before or after the checkbox.  Default FALSE
//        'label_title'   The text to be used for the title attribute for the field label
//        'id'            The id of the element.  Defaults to be the same as the name.
//        'value'         The value of the input.  Default ''
//        'class'         A class (or array of classes) to give the element.  Default NULL
//        'disabled'      Whether the field should be disabled.  Default FALSE
//        'create_hidden' Boolean.  If TRUE hidden inputs are created if 'disabled' is set
//                        Default TRUE
//        'mandatory'     Whether the field is a required field.  Default FALSE
//        'attributes'    Additional attributes not covered explicitly above.  Default NULL.
//                        Can be either a simple string or an array of attributes.
//
function generate_checkbox($params)
{
    // some sanity checking on params
    foreach (array('label', 'label_title', 'name', 'id', 'label_after', 'value', 'class',
                 'disabled', 'create_hidden', 'mandatory', 'attributes') as $key)
    {
        if (!isset($params[$key]))
        {
            switch ($key)
            {
                case 'label':
                case 'name':
                    trigger_error('Missing mandatory parameters', E_USER_NOTICE);
                    break;
                case 'id':
                    $params[$key] = $params['name'];
                    break;
                case 'value':
                case 'label_title':
                    $params[$key] = '';
                    break;
                case 'disabled':
                case 'label_after':
                case 'mandatory':
                    $params[$key] = FALSE;
                    break;
                case 'create_hidden':
                    $params[$key] = TRUE;
                    break;
                case 'class':
                    $params[$key] = array();
                    break;
                default:
                    break;
            }
        }
    }

    if (isset($params['attributes']) && is_array($params['attributes']))
    {
        $params['attributes'] = implode(' ', $params['attributes']);
    }

    if (!is_array($params['class']))
    {
        $params['class'] = array($params['class']);
    }
    $params['class'][] = 'checkbox';

    // generate the HTML
    // no HTML escaping for the label - it is trusted
    $html  = "<label for=\"" . $params['id'] . "\"";
    $html .= (empty($params['label_title'])) ? '' : ' title="' . htmlspecialchars($params['label_title']) . '"';
    $html .= ($params['label_after']) ? ' class="secondary"' : '';
    $html .= ">";
    if (!$params['label_after'])
    {
        $html .= $params['label'] . "</label>";
    }
    $html .= "<input type=\"checkbox\"";
    $html .= (count($params['class']) > 0) ? ' class="' . implode(' ', $params['class']) . '"' : '';
    $html .= " id=\"" . $params['id'] . "\" name=\"" . $params['name'] . "\" value=\"1\"";
    $html .= (empty($params['value'])) ? "" : " checked=\"checked\"";
    $html .= ($params['disabled']) ? " disabled=\"disabled\"" : "";
    $html .= ($params['mandatory']) ? " required aria-required=\"true\"" : "";
    $html .= (isset($params['attributes'])) ? " " . $params['attributes'] : '';
    $html .= ">";
    if ($params['label_after'])
    {
        $html .= $params['label'] . "</label>";
    }
    $html .= "\n";

    // and a hidden input if the input box is disabled and the value is true
    // (the checkbox isn't posted if not checked)
    if (($params['disabled']) && $params['create_hidden'] && !empty($params['value']))
    {
        $html .= "<input type=\"hidden\" value=\"1\" name=\"" . $params['name'] . "\">\n";
    }

    echo $html;
}

// Get localized field name for a user defined table column
// Looks for a tag of the format tablename.columnname (where tablename is
// stripped of the table prefix) and if can't find a string for that tag will
// return the columnname
function get_loc_field_name($table, $name)
{
    global $vocab, $db_tbl_prefix;
    $tag = utf8_substr($table, utf8_strlen($db_tbl_prefix));  // strip the prefix off the table name
    $tag .= "." . $name;           // add on the fieldname
    // then if there's a string in the vocab array for $tag use that
    // otherwise just use the fieldname
    return (isset($vocab[$tag])) ? get_vocab($tag) : $name;
}

// UTF-8 compatible substr function obtained from a contribution by
// "frank at jkelloggs dot dk" in the PHP online manual for substr()
function utf8_substr_old($str,$start)
{
    preg_match_all("/./su", $str, $ar);

    if(func_num_args() >= 3) {
        $length = func_get_arg(2);
        return join("", array_slice($ar[0], $start, $length));
    } else {
        return join("", array_slice($ar[0], $start));
    }
}

// UTF-8 compatible substr function
function utf8_substr($str, $start, $length=NULL)
{
    if (!isset($length))
    {
        // We cannot set $length to PHP_INT_MAX because there is a bug (#42101)
        // in mb_substr() when using PHP_INT_MAX on 64 bit Linux systems.  See
        // https://bugs.php.net/bug.php?id=42101.    Note also that passing NULL
        // for the length in mb_substr() causes mb_substr to use a length of 0.
        // See the user contributed notes on the PHP mb_substr() manual page.
        $length = utf8_strlen($str);
    }

    if (function_exists('mb_substr'))
    {
        // If we have mb_substr, use it - it's much quicker than our
        // routines, as it's native code

        $encoding = mb_detect_encoding($str);

        return mb_substr($str, $start, $length, $encoding);
    }
    if (strlen($str) > 1000)
    {
        // If the string is long, the old routine is quicker. :(

        return utf8_substr_old($str, $start, $length);
    }

    $i = 0;
    $index = 0;
    while ((ord($str[$index]) != 0) && ($i < $start))
    {
        $index = utf8_next_index($str, $index);
        $i++;
    }

    if (!isset($index))
    {
        return NULL;
    }
    if (func_num_args() >= 3)
    {
        $end_index = $index;

        $j = 0;
        while (isset($end_index) && ($j < $length))
        {
            $end_index = utf8_next_index($str, $end_index);
            $j++;
        }
        $j = 0;
        $ret = "."; // dummy to fool PHP
        for ($i = $index;
             (ord($str[$i]) != 0) && (!isset($end_index) || ($i < $end_index));
             $i++)
        {
            $ret[$j++] = $str[$i];
        }
        return $ret;
    }
    else
    {
        $j = 0;
        $ret = "."; // dummy to fool PHP
        for ($i = $index; ord($str[$i]) != 0; $i++)
        {
            $ret[$j++] = $str[$i];
        }
        return $ret;
    }
}

// Takes a UTF-8 string and a byte index into that string, and
// returns the byte index of the next UTF-8 sequence. When the end
// of the string is encountered, the function returns NULL
function utf8_next_index($str, $start)
{
    $ret = NULL;

    $i = $start;

    if (isset($str))
    {
        if (ord($str[$i]) < 0xc0)
        {
            $i++;
        }
        else
        {
            $i++;
            while ((ord($str[$i]) & 0xc0) == 0x80)
            {
                $i++;
            }
        }
        if (isset($str[$i]) && (ord($str[$i]) != 0))
        {
            $ret = $i;
        }
    }
    return $ret;
}

// Takes a UTF-8 string, and returns the number of _characters_ in the
// string
function utf8_strlen($str)
{
    if (!isset($str) || ($str == ""))
    {
        return 0;
    }
    if (function_exists('mb_strlen'))
    {
        // If we have mb_strlen(), use it - it'll be quicker
        return mb_strlen($str);
    }
    $len = 1;
    $next = 0;
    while ($next = utf8_next_index($str, $next))
    {
        $len++;
    }
    return $len;
}

// Generate an input which will be a <select> element if $select_options is set
// for the field, otherwise a <datalist> element if $datalist_options is set,
// otherwise an ordinary <input> field.
//
//   $params    an associative array holding the function parameters:
//      MANDATORY
//        'name'          The name of the input.
//      OPTIONAL
//        'field'         The name of the field, eg 'entry.name' as used by
//                        $select_options and $datalist_options
//        'label'         The text to be used for the field label.
//        'label_title'   The text to be used for the title attribute for the field label
//        'value'         The value of the input.  Default ''
//        'suffix'        A string that is displayed after the input field
//        'disabled'      Whether the field should be disabled.  Default FALSE
//        'create_hidden' Boolean.  If TRUE hidden inputs are created if 'disabled' is set
//                        Default TRUE
//        'mandatory'     Whether the field is a required field.  Default FALSE
//        'maxlength'     The maximum length of input allowed.   Default NULL (no limit)
//        'attributes'    Additional attributes, allowing HTML5 input types such as number and
//                        email to be used.   Note that additional attributes such as min, etc.
//                        can also be included in the string, eg 'type="number" min="1" step="1"'.
//                        Default NULL.  Can be either a simple string or an array of attributes
function generate_input($params)
{
    global $select_options, $datalist_options;

    if (isset($params['field']) && isset($select_options[$params['field']]))
    {
        $params['options']   = $select_options[$params['field']];
        generate_select($params);
    }
    elseif (isset($params['field']) && isset($datalist_options[$params['field']]))
    {
        $params['options']   = $datalist_options[$params['field']];
        generate_datalist($params);
    }
    else
    {
        generate_simple_input($params);
    }
}

// Generates a datalist element with an associated label
//
//   $params    an associative array holding the function parameters:
//      MANDATORY
//        'name'          The name of the element.
//      OPTIONAL
//        'id'            The id of the element.  Defaults to the same as the name.
//        'label'         The text to be used for the field label.
//        'label_title'   The text to be used for the title attribute for the field label
//        'options'       An array of options for the select element.   Can be a simple
//                        array or an associative array with value => text members for
//                        each <option> in the <select> element.   Default is an empty array.
//        'force_assoc'   Boolean.  Forces the options array to be treated as an
//                        associative array.  Default FALSE, ie it is treated as whatever
//                        it looks like.  (This parameter is necessary because if you
//                        index an array with strings that look like integers then PHP
//                        casts the keys to integers and the array becomes a simple indexed array)
//        'force_indexed' Boolean.  Forces the options array to be treated as a simple
//                        indexed array, ie just the values are used and the keys are
//                        ignored.   Default FALSE, ie it is treated as whatever it looks
//                        like.
//        'value'         The value of the input.  Default ''.
//        'disabled'      Whether the field should be disabled.  Default FALSE
//        'create_hidden' Boolean.  If TRUE hidden inputs are created if 'disabled' is set
//                        Default TRUE
//        'mandatory'     Whether the field is a required field.  Default FALSE
//        'attributes'    Additional attributes not covered explicitly above.  Default NULL.
//                        Can be either a simple string or an array of attributes.
//
function generate_datalist($params)
{
    global $HTTP_USER_AGENT;

    // some sanity checking on params
    foreach (array('label', 'label_title', 'name', 'id', 'options', 'force_assoc',
                 'force_indexed', 'value', 'disabled', 'create_hidden', 'mandatory',
                 'attributes') as $key)
    {
        if (!isset($params[$key]))
        {
            switch ($key)
            {
                case 'name':
                    trigger_error('Missing mandatory parameters', E_USER_NOTICE);
                    break;
                case 'id':
                    $params[$key] = $params['name'];
                    break;
                case 'options':
                    $params[$key] = array();
                    break;
                case 'value':
                case 'label_title':
                    $params[$key] = '';
                    break;
                case 'force_assoc':
                case 'force_indexed':
                case 'disabled':
                case 'mandatory':
                    $params[$key] = FALSE;
                    break;
                case 'create_hidden':
                    $params[$key] = TRUE;
                    break;
                default:
                    break;
            }
        }
    }

    if (isset($params['attributes']) && is_array($params['attributes']))
    {
        $params['attributes'] = implode(' ', $params['attributes']);
    }

    // generate the HTML
    // no HTML escaping for the label - it is trusted
    $html = '';
    if (isset($params['label']))
    {
        $html .= "<label for=\"" .$params['id'] . "\"";
        $html .= (empty($params['label_title'])) ? '' : ' title="' . htmlspecialchars($params['label_title']) . '"';
        $html .= ">" . $params['label'] . "</label>\n";
    }
    $html .= "<input id=\"" . $params['id'] . "\" name=\"" . $params['name'] . "\"";
    $html .= " list=\"" . $params['id'] . "_options\"";
    $html .= " value=\"" . $params['value'] . "\"";
    $html .= ($params['disabled']) ? " disabled=\"disabled\"" : "";
    $html .= (isset($params['pattern'])) ? " pattern=\"" . htmlspecialchars($params['pattern']) . "\"" : "";
    $html .= ($params['mandatory']) ? " required aria-required=\"true\"" : "";
    $html .= (isset($params['attributes'])) ? " " . $params['attributes'] : "";
    // We normally have the autocomplete atribute set to off because in most browsers
    // this stops the browser suggesting previous input and confines the list to our
    // options.   However in Opera turning off autocomplete turns off our options as
    // well, so we have to turn it on.  (We'll also turn it on for Opera in the JavaScript,
    // for good measure, just in case the server isn't passing through the user agent).
    $html .= " autocomplete=\"";
    $html .= (strpos(utf8_strtolower($HTTP_USER_AGENT), 'opera') === FALSE) ? 'off' : 'on';
    $html .= "\">\n";
    $html .= "<datalist id=\"" . $params['id'] . "_options\">";

    // Put a <select> wrapper around the options so that browsers that don't
    // support <datalist> will still have the options in their DOM and then
    // the JavaScript polyfill can find them and do something with them
    $html .= "<select style=\"display: none\">\n";

    foreach ($params['options'] as $value => $text)
    {
        // We can cope with both associative and ordinary arrays
        if (!$params['force_assoc'] && !is_assoc($params['options']))
        {
            $value = $text;
        }
        $html .= "<option";
        if (!$params['force_indexed'])
        {
            $html .= " value=\"" . htmlspecialchars($value) . "\"";
        }
        $html .= ">" . htmlspecialchars($text) . "</option>\n";
    }
    $html .= "</select>\n";
    $html .= "</datalist>\n";

    // and hidden inputs if the input is disabled
    if ($params['disabled'] && $params['create_hidden'])
    {
        $html .= "<input type=\"hidden\" name=\"" . $params['name'] . "\" value=\"".
            htmlspecialchars($params['value']) . "\">\n";
    }

    echo $html;
}

// Generate an ordinary input field, ie an <input>, with an associated label
//
//   $params    an associative array holding the function parameters:
//      MANDATORY
//        'name'          The name of the input.
//      OPTIONAL
//        'label'         The text to be used for the field label.
//        'label_title'   The text to be used for the title attribute for the field label
//        'value'         The value of the input.  Default ''
//        'type'          The type of input, eg 'text', 'number', etc.  Default NULL
//        'step'          The value of the 'step' attribute.  Default NULL
//        'min'           The value of the 'min' attribute.  Default NULL
//        'max'           The value of the 'max' attribute.  Default NULL
//        'suffix'        A string that is displayed after the input field
//        'disabled'      Whether the field should be disabled.  Default FALSE
//        'create_hidden' Boolean.  If TRUE hidden inputs are created if 'disabled' is set
//                        Default TRUE
//        'mandatory'     Whether the field is a required field.  Default FALSE
//        'maxlength'     The maximum length of input allowed.   Default NULL (no limit)
//        'attributes'    Additional attributes not covered explicitly above.  Default NULL.
//                        Can be either a simple string or an array of attributes.
//
function generate_simple_input($params)
{
    // some sanity checking on params
    foreach (array('label', 'label_title', 'name', 'value', 'type', 'step', 'disabled',
                 'create_hidden', 'mandatory', 'maxlength', 'attributes', 'suffix') as $key)
    {
        if (!isset($params[$key]))
        {
            switch ($key)
            {
                case 'name':
                    trigger_error('Missing mandatory parameters', E_USER_NOTICE);
                    break;
                case 'value':
                case 'label_title':
                    $params[$key] = '';
                    break;
                case 'disabled':
                case 'mandatory':
                    $params[$key] = FALSE;
                    break;
                case 'create_hidden':
                    $params[$key] = TRUE;
                    break;
                default:
                    break;
            }
        }
    }

    if (isset($params['attributes']) && is_array($params['attributes']))
    {
        $params['attributes'] = implode(' ', $params['attributes']);
    }

    // generate the HTML
    $html = '';
    if (isset($params['label']))
    {
        // no HTML escaping for the label - it is trusted
        $html .= "<label for=\"" . $params['name'] . "\"";
        $html .= (empty($params['label_title'])) ? '' : ' title="' . htmlspecialchars($params['label_title']) . '"';
        $html .= ">" . $params['label'] . "</label>\n";
    }
    $html .= "<input";
    $html .= (isset($params['type'])) ? " type=\"" . $params['type'] . "\"" : "";
    $html .= (isset($params['step'])) ? " step=\"" . $params['step'] . "\"" : "";
    $html .= (isset($params['min'])) ? " min=\"" . $params['min'] . "\"" : "";
    $html .= (isset($params['max'])) ? " max=\"" . $params['max'] . "\"" : "";
    $html .= (isset($params['pattern'])) ? " pattern=\"" . htmlspecialchars($params['pattern']) . "\"" : "";
    $html .= (isset($params['attributes'])) ? " " . $params['attributes'] : "";
    $html .= " id=\"" . $params['name'] . "\" name=\"" . $params['name'] . "\"";
    $html .= ($params['disabled']) ? " disabled=\"disabled\"" : '';
    $html .= ($params['mandatory']) ? " required aria-required=\"true\"" : '';
    $html .= (isset($params['maxlength'])) ? " maxlength=\"" . $params['maxlength'] . "\"" : '';
    // Don't give an empty string if it's a number as that's not a valid floating point number
    // and will fail HTML5 validation
    if (($params['value'] !== '') ||
        (isset($params['type']) && ($params['type'] != 'number')) )
    {
        $html .= " value=\"" . htmlspecialchars($params['value']) . "\"";
    }
    $html .= ">";
    if (isset($params['suffix']))
    {
        $html .= "<span>" . $params['suffix'] . "</span>";
    }
    $html .= "\n";
    // and a hidden input if the input box is disabled
    if ($params['disabled'] && $params['create_hidden'])
    {
        $html .= "<input type=\"hidden\" name=\"" . $params['name'] . "\" value=\"".
            htmlspecialchars($params['value'])."\">\n";
    }
    echo $html;
}

// Generate a single radio button (useful when you want to arrange the members
// of a radio group slightly differently from the standard way)
//
//   $params    an associative array holding the function parameters:
//      MANDATORY
//        'name'          The name of the input.
//      OPTIONAL
//        'value'         The value of the input.  Default ''
//        'options'       An associative array where the key is the value of the
//                        button and the value is the button text
//        'disabled'      Whether the field should be disabled.  Default FALSE
//        'create_hidden' Boolean.  If TRUE hidden inputs are created if 'disabled' is set
//                        Default TRUE
//
function generate_radio($params)
{
    // some sanity checking on params
    foreach (array('label', 'name', 'options', 'value', 'disabled', 'create_hidden') as $key)
    {
        if (!isset($params[$key]))
        {
            switch ($key)
            {
                case 'name':
                    trigger_error('Missing mandatory parameters', E_USER_NOTICE);
                    break;
                case 'options':
                    $params[$key] = array();
                    break;
                case 'value':
                    $params[$key] = '';
                    break;
                case 'disabled':
                    $params[$key] = FALSE;
                    break;
                case 'create_hidden':
                    $params[$key] = TRUE;
                    break;
                default:
                    break;
            }
        }
    }

    // get the value and the text for the button.   Although there should only
    // be one element in the options array we use an array for consistency
    // with the other functions
    $option = each($params['options']);
    // generate the HTML
    // no HTML escaping for the label - it is trusted
    $html  = "<label class=\"radio\">";
    $html .= "<input class=\"radio\" type=\"radio\" name=\"" . $params['name'] . "\" value=\"" . htmlspecialchars($option['key']) . "\"";
    $html .= ($params['value'] == $option['key']) ? " checked=\"checked\"" : "";
    $html .= ($params['disabled']) ? " disabled=\"disabled\"" : "";
    $html .= ">" . htmlspecialchars($option['value']);
    $html .= "</label>\n";

    // and a hidden input if the input box is disabled and the value is true
    // (the checkbox isn't posted if not checked)
    if ($params['disabled'] && $params['create_hidden'] && ($params['value'] == $option['key']))
    {
        $html .= "<input type=\"hidden\" value=\"" . htmlspecialchars($option['key']) . "\" name=\"" . $params['name'] . "\">\n";
    }
    echo $html;
}

// Generate a group of radio buttons with an associated label
//
//   $params    an associative array holding the function parameters:
//      MANDATORY
//        'label'         The text to be used for the field label.
//        'name'          The name of the input.
//        'options'       An array of options for the radio buttons.   Can be a simple
//                        array or an associative array with value => label members for
//                        each button.
//      OPTIONAL
//        'label_title'   The text to be used for the title attribute for the field label
//        'value'         The value of the input.  Defaults to the first element of 'options'
//                        (mirroring the behaviour of a <select> element)
//        'force_assoc'   Boolean.  Forces the options array to be treated as an
//                        associative array.  Default FALSE, ie it is treated as whatever
//                        it looks like.  (This parameter is necessary because if you
//                        index an array with strings that look like integers then PHP
//                        casts the keys to integers and the array becomes a simple array)
//        'disabled'      Whether the field should be disabled.  Default FALSE
//        'create_hidden' Boolean.  If TRUE hidden inputs are created if 'disabled' is set
//                        Default TRUE
//
function generate_radio_group($params)
{
    // some sanity checking on params
    // 'options' and 'force_assoc' must come before 'value' in the array
    foreach (array('label', 'label_title', 'name', 'options', 'force_assoc', 'value',
                 'disabled', 'create_hidden') as $key)
    {
        if (!isset($params[$key]))
        {
            switch ($key)
            {
                case 'label':
                case 'name':
                case 'options':
                    trigger_error('Missing mandatory parameters', E_USER_NOTICE);
                    break;
                case 'options':
                    $params[$key] = array();
                    break;
                case 'label_title':
                    $params[$key] = '';
                    break;
                case 'value':
                    if (is_assoc($params['options']) || $params['force_assoc'])
                    {
                        $array_keys = array_keys($params['options']);
                        $params[$key] = $array_keys[0];
                    }
                    else
                    {
                        $params[$key] = $params['options'][0];
                    }
                    break;
                case 'disabled':
                case 'force_assoc':
                    $params[$key] = FALSE;
                    break;
                case 'create_hidden':
                    $params[$key] = TRUE;
                    break;
                default:
                    break;
            }
        }
    }
    // generate the HTML
    // no HTML escaping for the label - it is trusted
    $html  = "<label";
    $html .= (empty($params['label_title'])) ? '' : ' title="' . htmlspecialchars($params['label_title']) . '"';
    $html .= ">" . $params['label'] . "</label>\n";
    $html .= "<div class=\"group\">\n";
    echo $html;

    // Output each radio button
    while ($option = each($params['options']))
    {
        // We can cope with both associative and ordinary arrays
        $button_label = $option['value'];
        if (is_assoc($params['options']) || $params['force_assoc'])
        {
            $button_value = $option['key'];
        }
        else
        {
            $button_value = $button_label;
        }
        generate_radio(array('name'          => $params['name'],
            'options'       => array($button_value => $button_label),
            'value'         => $params['value'],
            'disabled'      => $params['disabled'],
            'create_hidden' => $params['create_hidden']));
    }

    $html = "</div>\n";
    if ($params['disabled'] && $params['create_hidden'])
    {
        $html .= "<input type=\"hidden\" name=\"" . $params['name'] . "\"";
        $html .= " value=\"" . htmlspecialchars($params['value']) . "\">\n";
    }
    echo $html;
}

// Generate a group of checkboxes with an associated label
//
//   $params    an associative array holding the function parameters:
//      MANDATORY
//        'label'       The text to be used for the field label.
//        'name'        The name of the input.
//      OPTIONAL
//        'value'       The value of the input.  Can be an array. Default array()
//        'options'     An array of options for the checkboxes.   Can be a simple
//                      array or an associative array with value => label members for
//                      each checkbox.   Default is an empty array.
//        'force_assoc' Boolean.  Forces the options array to be treated as an
//                      associative array.  Default FALSE, ie it is treated as whatever
//                      it looks like.  (This parameter is necessary because if you
//                      index an array with strings that look like integers then PHP
//                      casts the keys to integers and the array becomes a simple array)
//        'disabled'    Whether the field should be disabled.  Default FALSE
//
function generate_checkbox_group($params)
{
    // some sanity checking on params
    foreach (array('label', 'name', 'options', 'force_assoc', 'value', 'disabled') as $key)
    {
        if (!isset($params[$key]))
        {
            switch ($key)
            {
                case 'label':
                case 'name':
                    trigger_error('Missing mandatory parameters', E_USER_NOTICE);
                    break;
                case 'options':
                    $params[$key] = array();
                    break;
                case 'value':
                    $params[$key] = array();
                    break;
                case 'force_assoc':
                case 'disabled':
                    $params[$key] = FALSE;
                    break;
                default:
                    break;
            }
        }
    }

    if (!is_array($params['value']))
    {
        $params['value'] = array($params['value']);
    }

    // generate the HTML
    // no HTML escaping for the label - it is trusted
    $html  = "<label>" . $params['label'] . "</label>\n";
    $html .= "<div class=\"group\">\n";
    // Output each checkbox
    foreach ($params['options'] as $value => $token)
    {
        // We can cope with both associative and ordinary arrays
        if (!$params['force_assoc'] && !is_assoc($params['options']))
        {
            $value = $token;
        }
        $html .= "<label>";
        $html .= "<input class=\"checkbox\" type=\"checkbox\" name=\"" . $params['name'] . "\"";
        $html .= " value=\"" . htmlspecialchars($value) . "\"";
        $html .= (in_array($value, $params['value'])) ? " checked=\"checked\"" : "";
        $html .= ($params['disabled']) ? " disabled=\"disabled\"" : "";
        $html .= ">" . htmlspecialchars($token);
        $html .= "</label>\n";
        if ($params['disabled'] && in_array($value, $params['value']))
        {
            $html .= "<input type=\"hidden\" name=\"" . $params['name'] . "\"";
            $html .= " value=\"" . htmlspecialchars($value) . "\">\n";
        }
    }
    $html .= "</div>\n";

    echo $html;
}

// Get the local day name based on language. Note 2000-01-02 is a Sunday.
function day_name($daynumber, $format=NULL)
{
    global $strftime_format;

    if (!isset($format))
    {
        $format = $strftime_format['dayname'];
    }

    return utf8_strftime($format, mktime(0,0,0,1,2+$daynumber,2000));
}

// Convert an RFC 5545 day to an ordinal number representing the day of the week,
// eg "MO" returns "1"
function RFC_5545_day_to_ord($day)
{
    global $RFC_5545_days;

    $tmp = array_keys($RFC_5545_days, $day);
    return $tmp[0];
}

// Converts a BYDAY (eg "2SU") value for a given year and month into a
// day of the month.   Returns FALSE if the day does not exist (eg for "5SU"
// when there are only four Sundays in the month)
function byday_to_day($year, $month, $byday)
{
    // First of all normalise the month and year, as we allow $month > 12
    while ($month > 12)
    {
        $month -= 12;
        $year++;
    }
    // Get the ordinal number and the day of the week
    list($ord, $dow) = byday_split($byday);
    // Get the starting day of the month
    $start_dom = ($ord > 0) ? 1 : date('t', mktime(0, 0, 0, $month, 1, $year));
    // Get the starting day of the week
    $start_dow = date('w', mktime(0, 0, 0, $month, $start_dom, $year));
    // Turn the BYDAY day of the week into an integer
    $byday_dow = RFC_5545_day_to_ord($dow);
    // get the difference in days
    $diff = $byday_dow - $start_dow;
    $diff += ($ord > 0) ? 7 : -7;
    $diff = $diff %7;
    // add in the weeks
    $diff += ($ord > 0) ? ($ord - 1) * 7 : ($ord + 1) * 7;

    $day = $start_dom + $diff;

    if (checkdate($month, $day, $year))
    {
        return $day;
    }
    else
    {
        return FALSE;
    }
}

// Converts a duration of $dur seconds into a duration of
// $dur $units
function toTimeString(&$dur, &$units, $translate=TRUE)
{
    if (abs($dur) >= 60)
    {
        $dur /= 60;

        if (abs($dur) >= 60)
        {
            $dur /= 60;

            if((abs($dur) >= 24) && ($dur % 24 == 0))
            {
                $dur /= 24;

                if((abs($dur) >= 7) && ($dur % 7 == 0))
                {
                    $dur /= 7;

                    if ((abs($dur) >= 52) && ($dur % 52 == 0))
                    {
                        $dur  /= 52;
                        $units = "years";
                    }
                    else
                    {
                        $units = "weeks";
                    }
                }
                else
                {
                    $units = "days";
                }
            }
            else
            {
                $units = "hours";
            }
        }
        else
        {
            $units = "minutes";
        }
    }
    else
    {
        $units = "seconds";
    }

    // Limit any floating point values to three decimal places
    if (is_float($dur))
    {
        $dur = sprintf('%.3f', $dur);
        $dur = rtrim($dur, '0');  // removes trailing zeros
    }

    // Translate into local language if required
    if ($translate)
    {
        $units = get_vocab($units);
    }
}

// Set and restore ignore_user_abort.   The function is designed to be used to
// ensure a critical piece of code can't be aborted, and used in pairs of set and
// restore calls.  The function keeps track of outstanding set requests so that
// the original state isn't restored if there are other requests still outstanding.
//
// $set   TRUE    set ignore_user_abort
//        FALSE   restore to the original state, if no other requests outstanding
function mrbs_ignore_user_abort($set)
{
    static $original_state;
    static $outstanding_requests = 0;

    if (!isset($original_state))
    {
        $original_state = ignore_user_abort();
    }

    // Set ignore_user_abort
    if ($set)
    {
        if ($outstanding_requests == 0)
        {
            ignore_user_abort(1);
        }
        $outstanding_requests++;
    }
    else
        // Restore the original state, provided no other requests are outstanding
    {
        $outstanding_requests--;
        if ($outstanding_requests == 0)
        {
            ignore_user_abort($original_state);
        }
    }
}