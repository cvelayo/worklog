<?php
global $mysqli;
$mysqli = new mysqli("localhost","worklog","Wardenburg","worklog");
if(mysqli_connect_errno()){
    trigger_error('Connection failed: '.$mysqli->error);
}

?>