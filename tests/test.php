<?php

$files = glob('tests/shake/*.rsp');

$ALGOS = array(
    'SHAKE128' => '--shake128',
    'SHAKE256' => '--shake256',
);

$return = 0;

foreach ($files as $file) {

    preg_match('#tests/\w+/(\w+\d+)#', $file, $matches);
    $algo = $ALGOS[$matches[1]];
    $bytes = 0;
    $intlen = 0;
    $msg = '';
    $out = '';

    $fd = fopen($file, 'r');

    echo "Processing $file...\n";
    while ($line = fgets($fd)) {
        $line = trim($line);
        if (strpos($line, '#') === 0) {
            continue; // comment, skip
        }
        if (empty($line)) {
            continue; // blank line, skip
        }
        if (strpos($line, 'COUNT = ') === 0) {
            continue; // test count, skip
        }
        if (strpos($line, '[Tested for') === 0) {
            continue; // information msg, skip
        }
        if (strpos($line, '[Minimum Output Length (bits) =') === 0) {
            continue; // information msg, skip
        }
        if (strpos($line, '[Maximum Output Length (bits) =') === 0) {
            continue; // information msg, skip
        }

        if (preg_match('/^\[?Outputlen = (\d+)\]?$/', $line, $matches)) {
            $bytes = intval($matches[1]) / 8;
        }

        if (preg_match('/^Len = (\d+)$/', $line, $matches)) {
            $inlen = (intval($matches[1]) / 8) * 2;
        }

        if (preg_match('/^[Input Length = (\d+)]$/', $line, $matches)) {
            $inlen = (intval($matches[1]) / 8) * 2;
        }

        if (preg_match('/^Msg = ([0123456789abcdef]+)/', $line, $matches)) {
            $msg = $matches[1];
        }

        if (preg_match('/^Output = ([0123456789abcdef]+)$/', $line, $matches)) {
            $hash = array();
            $out = $matches[1];
            $msg = substr($msg, 0, $inlen);
            $tmp = tempnam(sys_get_temp_dir(), 'shake-build-');
            file_put_contents($tmp, $msg);
            $cmd = './sha3sum --hex '.$algo.' --bytes '.$bytes." --quiet $tmp";
            $hash = trim(shell_exec($cmd));
            if ($hash !== $out) {
                $return = 1;
                echo "Failure: $cmd (expecting: $out, got $hash)\n";
            }
        }

    }
    fclose($fd);
}

exit($return);

