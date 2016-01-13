<html><head><title>SQL Server Actor Table Viewer</title></head><body>
<?php
/**
 * Created by PhpStorm.
 * User: tanni
 * Date: 1/11/2016
 * Time: 1:16 PM
 */
$db_host = '.\SQLEXPRESS';
$db_user = 'worklog';
$db_pwd = 'worklog';
$database = 'worklog';

global $conn;
$connectionInfo = array("UID" => $db_user, "PWD" => $db_pwd, "Database"=>$database, "CharacterSet" => 'UTF-8');
$conn = sqlsrv_connect( $db_host, $connectionInfo);

$sql = "SELECT [user], [start_time], [end_time] from times";
$result = sqlsrv_query($conn, $sql);

echo "<table >";
echo "<td style='solid black;Font-size=28;Font-Weight=bold'>";
echo "Table :   ". $table . "</td>";
echo "</table>";
echo "<table >";
echo "<tr>";

// printing table headers with desired column names
echo "<td style='border=1px solid black;Font-size=18;Font-Weight=bold'>";
echo "user";
echo "</td>";
echo "<td style='border=1px solid black;Font-size=18;Font-Weight=bold'>";
echo "start_time";
echo "</td>";
echo "<td style='border=1px solid black;Font-size=18;Font-Weight=bold'>";
echo "end_time";
echo "</td>";
echo "<td style='border=1px solid black;Font-size=18;Font-Weight=bold'>";
echo "start_time adjusted";
echo "</td>";
echo "<td style='border=1px solid black;Font-size=18;Font-Weight=bold'>";
echo "end_time adjusted";
echo "</td>";
echo "</tr>";

while ($row = sqlsrv_fetch_array($result))
{
    echo "<tr>";
    echo "<td style='border=1px solid black'>";
    echo $row['user'];
    echo "</td>";
    echo "<td style='border=1px solid black'>";
    echo $row['start_time'];
    echo "</td>";
    echo "<td style='border=1px solid black'>";
    echo $row['end_time'];
    echo "</td>";
    echo "<td style='border=1px solid black'>";
    echo date("Y-m-d T H:i:s \Z",$row['start_time']);
    echo "</td>";
    echo "<td style='border=1px solid black'>";
    echo date("Y-m-d\TH:i:s\Z",$row['end_time']);
    echo "</td>";
    echo "</tr>\n";
}
echo "</table>";

sqlsrv_free_stmt( $result);
sqlsrv_close( $conn);
?>
</body></html>