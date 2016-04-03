<?php

if ($argc != 4) {
    echo "Usage: ".$argv[0]." block_size directory total_bytes\n";
    exit(-1);
}

$_BLOCK_SIZE  = intval($argv[1]);
$_DIRECTORY   = rtrim($argv[2], '/');
$_TOTAL_BYTES = intval($argv[3]);

if ($_BLOCK_SIZE === 0) {
    echo "Error: invalid block size '$_BLOCK_SIZE'\n";
    exit(-1);
}

if ($_TOTAL_BYTES === 0) {
    echo "Error: invalid total size '$_TOTAL_BYTES'\n";
    exit(-1);
}

if (file_exists($_DIRECTORY)) {
    echo "Error: directory '$_DIRECTORY' already exists.\n";
    exit(-1);
}

if (!mkdir($_DIRECTORY, 0700, true)) {
    echo "Error: cannot create directory '$_DIRECTORY'.\n";
    exit(-1);
}

$i = 1;
while ($_TOTAL_BYTES > 0) {
    $len = $_TOTAL_BYTES;
    if ($_TOTAL_BYTES > $_BLOCK_SIZE) {
        $len = $_BLOCK_SIZE;
    }

    $s = substr(
        str_shuffle(str_repeat("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", $len)),
        0,
        $len
    );

    $file = sprintf($_DIRECTORY.'/%010u', $i);
    file_put_contents($file, $s);

    $_TOTAL_BYTES -= $len;
    $i++;
}
echo "$i files written.\n";
