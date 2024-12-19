<?php
$conn = mysqli_connect("localhost", "iot", "pwiot");
#   mysqli_set_charset($conn, "UTF-8");
mysqli_select_db($conn, "iotdb");

$query = "SELECT name, date, state, temp, humi FROM service";
$result = mysqli_query($conn, $query);

$data = array();

if ($result) {
    while ($row = mysqli_fetch_array($result)) {
        $name = $row['name'];
        if (!isset($data[$name])) {
            $data[$name] = array(array('Date', 'Temp', 'Humi'));
        }
        $data[$name][] = array(
            $row['date'],
            floatval($row['temp']),
            floatval($row['humi'])
        );
    }
}

$options = array(
    'title' => '공간별 온습도 분석',
    'width' => 1000,
    'height' => 400,
    'curveType' => 'function'
);

$json_data = json_encode($data);
$json_options = json_encode($options);
?>

<script src="//www.google.com/jsapi"></script>
<script>
var data = <?= $json_data ?>;
var options = <?= $json_options ?>;
google.load('visualization', '1.0', {'packages': ['corechart']});

google.setOnLoadCallback(function() {
    for (var name in data) {
        if (data.hasOwnProperty(name)) {
            var dataTable = new google.visualization.DataTable();
            dataTable.addColumn('string', 'Date');
            dataTable.addColumn('number', 'Temp');
            dataTable.addColumn('number', 'Humi');

            for (var i = 1; i < data[name].length; i++) {
                dataTable.addRow(data[name][i]);
            }

            var chartOptions = Object.assign({}, options, { title: name + ' 데이터 분석' });
            var chart = new google.visualization.LineChart(document.getElementById('chart_div_' + name));
            chart.draw(dataTable, chartOptions);
        }
    }
});
</script>

<?php foreach ($data as $name => $dataset) : ?>
    <div id="chart_div_<?= htmlspecialchars($name) ?>" style="margin-top: 50px;"></div>
<?php endforeach; ?>

<div id="chart_div"></div>

