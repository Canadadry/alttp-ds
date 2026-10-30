<?php
// Usage: php dump_pools.php <vt_dir> <out_dir> <case_dir>...
// Writes pool_lists.txt into <out_dir>. Exit 1, writing nothing, when a case's draws are not the first
// lines of its trace.txt.

const GETTERS = [
    'dungeon' => 'getDungeonPool',
    'advancement' => 'getAdvancementItems',
    'nice' => 'getNiceItems',
    'items' => 'getItemPool',
];

if ($argc < 4) {
    fwrite(STDERR, "usage: php dump_pools.php <vt_dir> <out_dir> <case_dir>...\n");
    exit(2);
}
[, $vt_dir, $out_dir] = $argv;
require __DIR__ . '/oracle_case.php';
$case_dirs = array_slice($argv, 3);
$cases = array_map('oracle_load_case', $case_dirs);
oracle_boot_vt($vt_dir);

$out = [
    '# VT\'s four item lists at attempt 0 of every case, written by dump_pools.php (run by gen_golden.sh),',
    '# which first checks its draws against the case\'s golden trace. Checked against the C pools by',
    '# test_generate --fixtures.',
    '# case <name> <seed> <key>=<value> for the 11 config.json keys',
    '# <dungeon|advancement|nice|items> <raw item names in VT\'s order>, in prepareWorld\'s getter order',
];
$records = 0;
foreach ($cases as $i => $case) {
    try {
        $world = oracle_attempt_world($case, 0);
        $lists = [];
        foreach (GETTERS as $list => $getter) {
            prng_trace_label($getter);
            $lists[$list] = array_map(fn ($item) => $item->getRawName(), $world->$getter());
            prng_trace_label(null);
        }
    } catch (\Throwable $e) {
        die_case($case['name'], get_class($e) . ': ' . $e->getMessage() . ' at ' . $e->getFile() . ':' . $e->getLine());
    }

    $draws = prng_trace();
    $golden = @file("{$case_dirs[$i]}/trace.txt", FILE_IGNORE_NEW_LINES);
    if ($golden === false) {
        die_case($case['name'], 'trace.txt is missing');
    }
    foreach ($draws as $n => $line) {
        if (($golden[$n] ?? null) !== $line) {
            die_case($case['name'], "draw " . ($n + 1) . ": dump has '$line', golden has '" . ($golden[$n] ?? 'nothing') . "'");
        }
    }
    $next_stage = explode(' ', $golden[count($draws)] ?? '0 0 -')[2];
    if ($next_stage === 'crystals' || in_array($next_stage, GETTERS, true)) {
        die_case($case['name'], 'draw ' . (count($draws) + 1) . ": golden has '{$golden[count($draws)]}', dump has nothing");
    }

    $options = array_map(fn ($key) => "$key={$case['config'][$key]}", CONFIG_KEYS);
    $out[] = implode(' ', ['case', $case['name'], $case['seed'], ...$options]);
    foreach ($lists as $list => $names) {
        $out[] = implode(' ', [$list, ...$names]);
        $records++;
    }
}

if (!is_dir($out_dir) && !mkdir($out_dir, 0777, true)) {
    fwrite(STDERR, "dump_pools: cannot create $out_dir\n");
    exit(1);
}
file_put_contents("$out_dir/pool_lists.txt", implode("\n", $out) . "\n");
echo 'dump_pools: ' . count($cases) . " cases, $records lists\n";
