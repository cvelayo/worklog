<?php
/**
 * Created by PhpStorm.
 * User: tanni
 * Date: 1/8/2016
 * Time: 12:17 AM
 */

$db_host = '.\SQLEXPRESS';
$db_user = 'worklog';
$db_pwd = 'worklog';
$database = 'worklog';
$table = 'actor';

$connectionInfo = array("UID" => $db_user, "PWD" => $db_pwd, "Database"=>$database);
$conn = sqlsrv_connect( $db_host, $connectionInfo);
if( !$conn )
{
    echo "Connection could not be established.\n";
    die( print_r( sqlsrv_errors(), true));
}
else
{ echo "Connected!";
}
if (!isset($_GET['user'])) {
    echo '<h1>Select your log</h1>';
    $sql = 'SELECT DISTINCT team
         FROM users';
    $result = sqlsrv_query($conn, $sql);
    if (!$result) {
        echo "fail";
        die(print_r(sqlsrv_errors(), true));
    }
    while ($row = sqlsrv_fetch_array($result)) {
        $team = $row['team'];
        if ($team != 'test') {
            echo '<h2>' . $team . '</h2>';
            $sql2 = "SELECT * FROM users WHERE team ='" . $team . "' ORDER BY name  ";

            $result2 = sqlsrv_query($conn, $sql2);
            if (!$result2) {
                echo "fail";
                die(print_r(sqlsrv_errors(), true));
            }
            while ($row2 = sqlsrv_fetch_array($result2)) {
                echo '<a href="temp.php?user=' . $row2['code'] . '">' . $row2['name'] . '</a><br />';
            }
        }
    }
    sqlsrv_free_stmt($result);
    sqlsrv_free_stmt($result2);
}