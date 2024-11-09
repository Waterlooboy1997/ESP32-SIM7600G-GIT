//let example_data = {"sensor":"OPUS","headers":["Rec","Battery Voltage (v)","Water Level (m)","Nitrate-nitrogen (mg/L)","TSS","SQI","ABS360","ABS210","ABS254","UVT254","SAC254","Water Temp (degC)"],"data":[{"time":"2024-08-30 15:30:59","values":["10","13.160","0.010",0,0,0,0,0,0,0,0,"26.719"]},{"time":"2024-08-30 15:30:59","values":["10","13.160","0.010",0,0,0,0,0,0,0,0,"26.719"]},{"time":"2024-08-30 15:30:59","values":["10","13.160","0.010",0,0,0,0,0,0,0,0,"26.719"]},{"time":"2024-08-30 15:30:59","values":["10","13.160","0.010",0,0,0,0,0,0,0,0,"26.719"]},{"time":"2024-08-30 15:30:59","values":["10","13.160","0.010",0,0,0,0,0,0,0,0,"26.719"]},{"time":"2024-08-30 15:30:59","values":["10","13.160","0.010",0,0,0,0,0,0,0,0,"26.719"]}]}

// Modularized JavaScript
const DataloggerApp = (function () {
    // Global variables
    let esp32Server = true; // True if being hosted on the ESP32 and accessed using WiFi
    // Data Module
    const DataModule = {
        clearDatatable: function () {
            var table;
            if ($.fn.dataTable.isDataTable('#measurementDatatable')) {
                table = $('#measurementDatatable').DataTable();
                table.clear().draw();
                table.destroy();
            }
        },

        createDataTable: function (jsonData) {
            this.clearDatatable();

            // Prepare data for DataTable
            const tableData = jsonData.data.map(item => {
                return [
                    item.time,
                    ...item.values.map(value => typeof value === 'number' ? parseFloat(value.toFixed(3)) : value)
                ];
            });

            let cols = ["Timestamp"];

            for (let i = 0; i < jsonData.headers.length; i++) {
                cols.push(jsonData.headers[i]);
            }



            table = $('#measurementDatatable').DataTable({
                data: tableData,
                columns: cols.map(header => ({ title: header })),
                scrollX: true,
                scrollY: "300px",
                searching: false,
                paging: false,
                info: false,
                order: [[0, "desc"]], // Sort by first column (Rec) in descending order
            });
        },

        csvJSON: function (csv, rows) {
            const lines = csv.split("\r\n");
            const result = [];
            const headers = lines[0].split(",");

            for (let i = lines.length - 2; i > 0 && result.length < rows; i--) {
                let obj = {};
                const currentline = lines[i].split(",");
                for (let j = 0; j < headers.length; j++) {
                    obj[headers[j]] = currentline[j];
                }
                result.push(obj);
            }
            return result.reverse();
        }
    };

    // Main App Functions
    return {
        init: function () {
            this.bindEvents();
        },

        bindEvents: function () {
            $("#downloadMeasurementBtn").on("click", () => {
                alert("Downloading data...");
                this.requestMeasurementAndCreateDatatable($('#rowsToCollectInput').val());
            });
        },

        serverRequest: function (command, params) {
            return new Promise((resolve, reject) => {
                let url = esp32Server ? `/processCommand?command=${command}${params}` : `/${command}${params}`;
                console.log(url);
                $.ajax({
                    url: url,
                    type: "GET",
                    timeout: 20000,
                    success: function (data) {
                        try {
                            let parsedData = JSON.parse(data);
                            let prettyJson = JSON.stringify(parsedData, null, 2); // Convert JSON to a pretty string with indentation
                            $('#log').append(prettyJson + "\r\n");
                        } catch (err) {
                            $('#log').append(err + "\r\n");
                        }
                        resolve(data);
                    },
                    error: function (xhr, status, error) {
                        reject(error);
                    }
                });
            });
        },

        // Update the requestMeasurementAndCreateDatatable function in the main app
        requestMeasurementAndCreateDatatable: async function (rowsToCollect) {

            return new Promise((resolve, reject) => {
                try {
                    console.log("Requesting server measurement");
                    let sensorType = $('#sensorType').val();
                    this.serverRequest("printjson=" + sensorType + ",," + rowsToCollect, "").then(jsonDataRaw => {
                        let jsonData = JSON.parse(jsonDataRaw);
                        console.log(jsonData);
                        if (jsonData) { //&& jsonData.headers && jsonData.data) {
                            DataModule.createDataTable(jsonData);
                            resolve();
                        }

                    })
                } catch (e) {
                    reject(e);
                }
            })

        },
    };
})();

// Initialize the app when the document is ready
$(document).ready(function () {
    DataloggerApp.init();
});