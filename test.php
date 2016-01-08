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