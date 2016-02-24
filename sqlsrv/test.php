<?php
/**
 * Created by PhpStorm.
 * User: tanni
 * Date: 1/8/2016
 * Time: 12:17 AM
 */
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


global $conn;
$user = $_GET['user'];
print_header($day, $month, $year, $user);
echo "<div id=\"dwm_header\" class=\"screenonly\">\n";
echo make_area_select_html('test.php', $user, $year, $month, $day);
minicals($year, $month, $day, $area, $room, 'temp', $user);
if (!isset($_GET['user'])) {
    echo '<h1>Select your log</h1>';
    $sql = "SELECT DISTINCT team
         FROM users";
    $result = sqlsrv_query($conn, $sql);

    while ($row = sqlsrv_fetch_array($result)) {
        $team = $row['team'];
        if ($team != 'test') {
            echo '<h2>' . $team . '</h2>';
            $sql2 = "SELECT * FROM users WHERE team ='" . $team . "' ORDER BY name  ";

            $result2 = sqlsrv_query($conn, $sql2);
            while ($row2 = sqlsrv_fetch_array($result2)) {
                echo '<a href="test.php?user=' . $row2['code'] . '">' . $row2['name'] . '</a><br />';
            }
        }
    }
    sqlsrv_free_stmt($result);
    sqlsrv_free_stmt($result2);
}
else {

    $timetohighlight = get_form_var('timetohighlight', 'int');
    $this_user_name = get_area_name($user);
    echo "<div id=\"dwm\">\n";
    echo "<h2>". htmlspecialchars("$this_user_name")."</h2>\n";
    echo "</div>\n";
    $i = mktime(12, 0, 0, $month, $day - 7, $year);
    $yy = date("Y", $i);
    $ym = date("m", $i);
    $yd = date("d", $i);

    $i = mktime(12, 0, 0, $month, $day + 7, $year);
    $ty = date("Y", $i);
    $tm = date("m", $i);
    $td = date("d", $i);

// Show Go to week before and after links
    $before_after_links_html = "
<div class=\"screenonly\">
  <div class=\"date_nav\">
    <div class=\"date_before\">
      <a href=\"test.php?year=$yy&amp;month=$ym&amp;day=$yd&amp;user=$user\">
          &lt;&lt;&nbsp;" . get_vocab("weekbefore") . "
      </a>
    </div>
    <div class=\"date_now\">
      <a href=\"test.php?user=$user\">
          " . get_vocab("gotothisweek") . "
      </a>
    </div>
    <div class=\"date_after\">
      <a href=\"test.php?year=$ty&amp;month=$tm&amp;day=$td&amp;user=$user\">
          " . get_vocab("weekafter") . "&nbsp;&gt;&gt;
      </a>
    </div>
  </div>
</div>
";

    print $before_after_links_html;
    $inner_html = week_table_innerhtml($day, $month, $year, $user, $timetohighlight);
    echo "<table class=\"dwm_main\" id=\"week_main\" data-resolution=\"$resolution\">";
    echo $inner_html;
    echo "</table>\n";
    print $before_after_links_html;
    output_trailer();
}