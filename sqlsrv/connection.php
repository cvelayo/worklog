<?php
$db_host = '.\SQLEXPRESS';
$db_user = 'worklog';
$db_pwd = 'worklog';
$database = 'worklog';

global $conn;
$connectionInfo = array("UID" => $db_user, "PWD" => $db_pwd, "Database"=>$database, "CharacterSet" => 'UTF-8');
$conn = sqlsrv_connect( $db_host, $connectionInfo);
if( !$conn )
{
    echo "Connection could not be established.\n";
    die( print_r( sqlsrv_errors(), true));
}
?>