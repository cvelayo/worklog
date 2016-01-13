<?php
/**
 * Created by PhpStorm.
 * User: tanni
 * Date: 1/8/2016
 * Time: 12:40 PM
 */


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
        $result = $row;
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
