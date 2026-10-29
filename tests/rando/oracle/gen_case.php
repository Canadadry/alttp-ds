<?php
// Usage: php gen_case.php <vt_dir> <case_dir> <out_dir>
// Writes spoiler.json, trace.txt and attempts into <out_dir> only on success; exit 1 otherwise.

const WATCHDOG = 1000;
const CONFIG_KEYS = [
    'glitches', 'item_placement', 'dungeon_items', 'accessibility', 'goal', 'crystals_tower',
    'crystals_ganon', 'state', 'weapons', 'item_pool', 'item_functionality',
];

if ($argc !== 4) {
    fwrite(STDERR, "usage: php gen_case.php <vt_dir> <case_dir> <out_dir>\n");
    exit(2);
}
[, $vt_dir, $case_dir, $out_dir] = $argv;
$case = basename($case_dir);

function die_case(string $case, string $why): never
{
    fwrite(STDERR, "gen_case: $case: $why\n");
    exit(1);
}

$config = json_decode((string) @file_get_contents("$case_dir/config.json"), true);
if (!is_array($config)) {
    die_case($case, 'config.json is missing or not a JSON object');
}
foreach (CONFIG_KEYS as $key) {
    if (!isset($config[$key]) || !is_string($config[$key])) {
        die_case($case, "config.json: $key is missing or not a string");
    }
}
foreach (array_keys($config) as $key) {
    if (!in_array($key, CONFIG_KEYS, true)) {
        die_case($case, "config.json: unknown key $key");
    }
}
$seed_text = (string) @file_get_contents("$case_dir/seed");
if (!preg_match('/^[0-9]{1,10}\n?$/', $seed_text) || (int) $seed_text > 0xFFFFFFFF) {
    die_case($case, 'seed is not a decimal uint32 on one line');
}
$seed = (int) $seed_text;

ini_set('memory_limit', '512M');
require "$vt_dir/vendor/autoload.php";
$app = require "$vt_dir/bootstrap/app.php";
$app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();
config([
    'logging.channels.oracle' => ['driver' => 'monolog', 'handler' => Monolog\Handler\NullHandler::class],
    'logging.default' => 'oracle',
]);

$logic = [
    'none' => 'NoGlitches',
    'overworld_glitches' => 'OverworldGlitches',
    'hybrid_major_glitches' => 'HybridMajorGlitches',
    'major_glitches' => 'MajorGlitches',
    'no_logic' => 'NoLogic',
][$config['glitches']] ?? die_case($case, "config.json: unknown glitches {$config['glitches']}");
$item_placement = $config['item_pool'] === 'crowd_control' ? 'advanced' : $config['item_placement'];

function normalize(array $a): array
{
    foreach ($a as $k => $v) {
        if (is_array($v)) {
            $a[$k] = normalize($v);
        }
    }
    if (!array_is_list($a)) {
        ksort($a, SORT_STRING);
    }
    return $a;
}

// Seed::boot() builds a Hashids, which needs bcmath or gmp, only for a DB-save listener the oracle
// never fires. Its throw still marks Seed booted, so World::factory() no longer throws.
if (!extension_loaded('bcmath') && !extension_loaded('gmp')) {
    try {
        new ALttP\Seed;
    } catch (RuntimeException $e) {
    }
}

$max_world = new ReflectionProperty(ALttP\World::class, 'max_world');
$trace = [];
for ($attempt = 0; $attempt < WATCHDOG; ++$attempt) {
    ALttP\Item::clearCache();
    ALttP\Boss::clearCache();
    $max_world->setValue(null, 1);
    prng_trace_start($attempt);
    prng_seed(prng_splitmix32(($seed + $attempt) & 0xFFFFFFFF));
    try {
        // prepSeed's order: ganon, then tower.
        prng_trace_label('crystals');
        $crystals_ganon = $config['crystals_ganon'] === 'random' ? get_random_int(0, 7) : $config['crystals_ganon'];
        $crystals_tower = $config['crystals_tower'] === 'random' ? get_random_int(0, 7) : $config['crystals_tower'];
        prng_trace_label(null);

        $world = ALttP\World::factory($config['state'], [
            'itemPlacement' => $item_placement,
            'dungeonItems' => $config['dungeon_items'],
            'accessibility' => $config['accessibility'],
            'goal' => $config['goal'],
            'crystals.ganon' => $crystals_ganon,
            'crystals.tower' => $crystals_tower,
            'entrances' => 'none',
            'mode.weapons' => $config['weapons'],
            'tournament' => false,
            'spoilers' => 'on',
            'allow_quickswap' => true,
            'override_start_screen' => false,
            'pseudoboots' => false,
            'spoil.Hints' => 'on',
            'logic' => $logic,
            'item.pool' => $config['item_pool'],
            'item.functionality' => $config['item_functionality'],
            'enemizer.bossShuffle' => 'none',
            'enemizer.enemyShuffle' => 'none',
            'enemizer.enemyDamage' => 'default',
            'enemizer.enemyHealth' => 'default',
            'enemizer.potShuffle' => 'off',
        ]);
        $rand = new ALttP\Randomizer([$world]);
        // Only randomize() and isWinnable() may fail an attempt: an exception elsewhere is a harness bug.
        try {
            $rand->randomize();
            $won = (new ALttP\Support\WorldCollection($rand->getWorlds()))->isWinnable();
        } catch (\Exception $e) {
            $won = false;
        }
    } catch (\Throwable $e) {
        die_case($case, "attempt $attempt: " . get_class($e) . ': ' . $e->getMessage()
            . ' at ' . $e->getFile() . ':' . $e->getLine());
    }
    array_push($trace, ...prng_trace());
    if ($won) {
        break;
    }
}
if ($attempt === WATCHDOG) {
    die_case($case, 'no winnable attempt within ' . WATCHDOG . ' attempts');
}

$spoiler = $world->getSpoiler([
    'entry_crystals_ganon' => $config['crystals_ganon'],
    'entry_crystals_tower' => $config['crystals_tower'],
    'worlds' => 1,
]);
unset($spoiler['meta']['build']);
$json = json_encode(normalize($spoiler), JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
    | JSON_THROW_ON_ERROR);

if (!is_dir($out_dir) && !mkdir($out_dir, 0777, true)) {
    die_case($case, "cannot create $out_dir");
}
file_put_contents("$out_dir/spoiler.json", "$json\n");
file_put_contents("$out_dir/trace.txt", $trace === [] ? '' : implode("\n", $trace) . "\n");
file_put_contents("$out_dir/attempts", "$attempt\n");
if (realpath($out_dir) !== realpath($case_dir)) {
    copy("$case_dir/config.json", "$out_dir/config.json");
    copy("$case_dir/seed", "$out_dir/seed");
}
echo "gen_case: $case: attempts $attempt, " . count($trace) . " draws\n";
