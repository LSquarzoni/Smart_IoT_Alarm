<?php
require 'vendor/autoload.php';
use InfluxDB2\Client;
use InfluxDB2\Model\WritePrecision;
use InfluxDB2\Point;
use InfluxDB2\WriteApi;

# You can generate an API token from the "API Tokens Tab" in the UI
$token = 'TOKEN';
$org = 'ORG';
$bucket = 'BUCKET';

$client = new Client([
    "url" => "URL",
    "token" => $token,
]);

if ($_SERVER['REQUEST_METHOD'] == 'POST') {
    $data = file_get_contents('php://input');
    $timestamp = date('Y-m-d H:i:s');
    $timestamp = date('Y-m-d H:i:s', strtotime($timestamp . ' +2 hours'));

    // Save to CSV
    $file = fopen('ESP32_data.csv', 'a');

    if ($file) {
        fputcsv($file, array($timestamp, $data));
        fclose($file);
        echo "Data received and saved to CSV\n";
    } else {
        echo "Failed to open CSV file\n";
    }

    // Save to InfluxDB
    try {
        $writeApi = $client->createWriteApi();
        $point = Point::measurement('pressure_data')
            ->addTag('source', 'ESP32')
            ->addField('value', floatval($data))
            ->time(microtime(true));

        $writeApi->write($point, WritePrecision::S, $bucket, $org);
        echo "Data sent to InfluxDB\n";
    } catch (Exception $e) {
        echo "Failed to write to InfluxDB: " . $e->getMessage();
    }
} else {
    echo "Send a POST request with data\n";
}

$client->close();
?>
