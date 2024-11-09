// Modularized JavaScript
const DataloggerApp = (function() {
    // Global variables
    let scanRate = 5000;
    let maximumTimeOnline = 300000;
    let timeoutClock = maximumTimeOnline;
    let timeoutReached = false;
    let pollTimerGlobal;
    let lastConfig;
    let toPoll = true;
    let waitingServerRes = false;
    let esp32Server = true; // True if being hosted on the ESP32 and accessed using WiFi
    let changes = {};
    let changesStingify; 
    // Config Module
    const ConfigModule = {
        createConfigUI: function(config) {
            let html = '';
            console.log("Config:");
            console.log(config)
            lastConfig = config
            for (const [key, value] of Object.entries(config.data)) {
                if (typeof value === 'object' && value !== null) {
                    html += `<div class="config-group"><h3>${key}</h3>`;
                    html += this.createConfigRows(value, key);
                    html += `</div>`;
                } else {
                    html += this.createConfigRow(key, value);
                }
            }
            return html;
        },

        createConfigRows: function(obj, parentKey) {
            let html = '';
            for (const [key, value] of Object.entries(obj)) {
                if (typeof value === 'object' && value !== null) {
                    html += this.createConfigRows(value, `${parentKey}.${key}`);
                } else {
                    html += this.createConfigRow(key, value, parentKey);
                }
            }
            return html;
        },

        createConfigRow: function(key, value, parentKey = '') {
            const fullKey = parentKey ? `${parentKey}.${key}` : key;
            const isEditable = !['tsnow', 'battV', 'lipo', 'draw', 'meas', 'rtcinit', 'espRtcInit', 'fsFill'].includes(key);
            return `
                <div class="config-item">
                    <label for="${fullKey}">${key}:</label>
                    ${this.createInput(fullKey, value, isEditable)}
                </div>
            `;
        },

        createInput: function(key, value, isEditable) {
            const type = typeof value;
            const disabledAttr = isEditable ? '' : 'disabled';
            switch (type) {
                case 'boolean':
                    return `<select class="config-dropdown" id="${key}" ${disabledAttr}>
                        <option value="true" ${value ? 'selected' : ''}>true</option>
                        <option value="false" ${!value ? 'selected' : ''}>false</option>
                    </select>`;
                case 'number':
                    return `<input type="number" id="${key}" value="${value}" ${disabledAttr}>`;
                default:
                    return `<input type="text" id="${key}" value="${value}" ${disabledAttr}>`;
            }
        },

        updateConfigValues: function(obj, prefix) {
            for (const [key, value] of Object.entries(obj)) {
                const fullKey = prefix ? `${prefix}.${key}` : key;
                const element = document.getElementById(fullKey);
                if (element) {
                    const newValue = element.value;
                    obj[key] = typeof value === 'number' ? Number(newValue) :
                               typeof value === 'boolean' ? newValue === 'true' :
                               newValue;
                } else if (typeof value === 'object' && value !== null) {
                    this.updateConfigValues(value, fullKey);
                }
            }
        },

        findDifferences: function(obj1, obj2) {
            const diff = {};
            const updatePromises = [];
    
            const findDiffRecursive = (o1, o2, path = '') => {
                for (const key in o1) {
                    if (o1.hasOwnProperty(key)) {
                        const newPath = path ? `${path}/${key}` : key;
                        if (typeof o1[key] === 'object' && o1[key] !== null) {
                            findDiffRecursive(o1[key], o2[key], newPath);
                        } else if (o1[key] !== o2[key]) {
                            diff[newPath] = { old: o1[key], new: o2[key] };
                            diff[newPath].command = `updatejson=${newPath},${o2[key]}`
                            // Create a promise for the server update
                            //updatePromises.push(
                                //DataloggerApp.serverRequest(`updatejson=${newPath},${o2[key]}`, "")
                            //);
                        }
                    }
                }
            };
    
            findDiffRecursive(obj1, obj2);
    
            // Wait for all update promises to resolve
            //Promise.all(updatePromises)
    
            return diff;
        },
    };

    // Data Module
    const DataModule = {
        clearDatatable: function() {
            var table;
            if ($.fn.dataTable.isDataTable('#measurementDatatable')) {
                table = $('#measurementDatatable').DataTable();
                table.clear().draw();
                table.destroy();
            }
        },

        createDataTable: function(jsonData) {
            this.clearDatatable();
            
            // Prepare data for DataTable
            const tableData = jsonData.data.map(item => {
                return [
                    item.time, 
                    ...item.values.map(value => typeof value === 'number' ? parseFloat(value.toFixed(3)) : value)
                ];
            });

            let cols = ["Timestamp"];

                for(let i = 0; i < jsonData.headers.length; i++){
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

        csvJSON: function(csv, rows) {
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
        init: function() {
            this.bindEvents();
            $('#scanRate').val(scanRate);
            $('#timeoutValue').html(0);
            pollTimerGlobal = setInterval(this.poll, scanRate);
        },

        bindEvents: function() {
            $("#scanRate").on("change", () => {
                clearInterval(pollTimerGlobal);
                scanRate = Number($("#scanRate").val());
                if (scanRate < 1000) {
                    scanRate = 1000;
                }
                pollTimerGlobal = setInterval(this.poll, scanRate);
            });

            $('#resetTimeoutBtn').on('click', () => {
                $('#timeoutValue').html(maximumTimeOnline);
            });

            $('#sendPollBtn').on("click", () => {
                this.serverRequest("poll", "").then(data => {
                    console.log(data);
                }).catch(err => console.log(err));
            });

            $('#toPoll').on('change',()=>{
                let booltext = $('#toPoll option:selected').text();
                console.log(booltext)
                if(booltext == "True"){
                    toPoll = true;
                } else {
                    toPoll = false;
                }
            })

            $('#sendCustomCommand').on("click", () => {
                let customCommand = $('#customCommandInput').val();
                this.serverRequest(customCommand, "").then(data => {
                    console.log(data);
                }).catch(err => console.log(err));
            });

            $("#downloadMeasurementBtn").on("click", () => {
                alert("Downloading data...");
                this.requestMeasurementAndCreateDatatable($('#rowsToCollectInput').val());
            });

            $("#resetBtn").on("click", () => {
                console.log("Sending reset");
                this.serverRequest("reset", "").then(data => {
                    // Handle Reset
                }).catch(err => console.log(err));
            });

            $('#presetCommands').on("change", function() {
                console.log($(this).val());
                $('#customCommandInput').val($(this).val());
            });

            $("#startMeasurementBtn").on("click", () => {
                alert("Triggering measurement...");
                this.serverRequest("triossetmeasurement", "").then(data => {
                    // Handle Start Measurement
                }).catch(err => console.log(err));
            });

            // Update the calling function
            $('#updateConfig').on('click', () => {
                console.log('Original');
                //console.log(JSON.stringify(lastConfig, null, 2));
                const updatedConfig = JSON.parse(JSON.stringify(lastConfig));
                ConfigModule.updateConfigValues(updatedConfig.data, '');
                console.log('New');
//                console.log(JSON.stringify(updatedConfig, null, 2));

                changes = ConfigModule.findDifferences(lastConfig.data, updatedConfig.data);
                console.log('Changed Properties:');
                let changesStringify = JSON.stringify(changes, null, 2)
                console.log(changesStringify);
                let changesHtml = '';
                Object.keys(changes).forEach(key => {
                    changesHtml += `<label><input type="checkbox" class="change-checkbox" value="${key}" checked> ${key}: ${JSON.stringify(changes[key], null, 2)}</label><br>`;
                });
                $('#changesList').html(changesHtml);
                $('#confirmModal').show();

                // After all updates are done, refresh the config
                //Promise.all(Object.values(changes).map(() => Promise.resolve()))
            });

            $('#confirmChanges').on('click', () => {
                // Gather selected changes
                let selectedChanges = {};
                $('.change-checkbox:checked').each(function() {
                    let key = $(this).val();
                    selectedChanges[key] = changes[key];
                });
            
                let updatePromises = [];
            
                // Loop through selected changes and create promises
                for (let key in selectedChanges) {
                    if (selectedChanges.hasOwnProperty(key)) {
                        let command = selectedChanges[key].command;
                        updatePromises.push(
                            DataloggerApp.serverRequest(command, "")
                        );
                    }
                }
            
                // Proceed with applying the selected changes using the generated promises
                console.log(updatePromises);
                Promise.all(updatePromises)
                    .then(() => {
                        console.log('All changes applied successfully.');
                        // You can also add any additional logic here if needed after all promises are resolved
                    })
                    .catch((err) => {
                        console.error('Error applying changes:', err);
                        // Handle errors if any of the promises fail
                    });
            
                // Hide the modal
                $('#confirmModal').hide();
            });

            // Event listener for canceling changes
            $('#cancelChanges').on('click', () => {
                // Hide the modal without applying changes
                $('#confirmModal').hide();
            });

            $('#getConfigBtn').on('click', () => {
                this.getConfig("server");
            });

            $('#updateTimestampBtn').on('click', () => {
                let unixTimestamp = moment().unix();
                unixTimestamp = unixTimestamp + 36000;
                console.log("Updating timestamp to: " + unixTimestamp);
                this.serverRequest("settimestamp" + unixTimestamp, "").then(data => {
                    // Handle Start Measurement
                }).catch(err => console.log(err));
            });

            $('#hideConfig').on('click', () => {
                try{
                    $('#config-container').css("display","none");
                }catch(err){console.log(err)}
                
            });

            $('#showConfig').on('click', () => {
                try{
                    $('#config-container').css("display","block");
                }catch(err){console.log(err)}
            });

            $('#clearLog').on('click', () => {
                try{
                    console.log("Clearing log..");
                    $('#log').empty();
                }catch(err){console.log(err)}
            });

            const intervalId = setInterval(() => {
                $("#updateTimestampBtn").click();
            }, 15000);
            clearInterval(intervalId);
        },

        checkAndPromptUpdateTimestamp: function(loggerTime) {
            // Get the current PC time in Unix format
            let currentPCTime = Math.floor(Date.now() / 1000);
            const currentPCTimeFormatted = moment.unix(currentPCTime).format("YYYY-MM-DD HH:mm:ss");

            // Get the logger time in unix 
            const loggerUnix = moment(loggerTime, "YYYY-MM-DD HH:mm:ss").unix();

            //Caculate difference between UNIX
            let timeDifference = Math.abs(currentPCTime - loggerUnix);
        
            // Define acceptable time difference threshold in seconds (e.g., 60 seconds)
            const timeThreshold = 60;

            if (timeDifference > timeThreshold) {
                // Show the confirmation modal with the time details
                alert("Automatically updating timestamp from autodetect...\r\nLogger time: " + loggerTime + "\r\nServer Time: " + currentPCTimeFormatted);
                console.log(loggerTime);
                console.log(currentPCTime);
                console.log(timeDifference);
                $('#updateTimestampBtn').click();
               // $('#confirmTimestampModal').find('.logger-time').text(loggerTime);
               // $('#confirmTimestampModal').find('.pc-time').text(currentPCTime);
               // $('#confirmTimestampModal').modal('show'); // Assuming you're using Bootstrap modal
            }
        },

        poll: async function () {
            toPoll
            if (toPoll) {
                timeoutClock -= scanRate;
                if (timeoutClock < 0) {
                    timeoutClock = 0;
                    timeoutReached = true;
                }
                $("#timeoutValue").html(timeoutClock);

                if (!timeoutReached) {
                    try {
                        let data = await DataloggerApp.serverRequest("poll", '')
                        const json = JSON.parse(data);
                        console.log(json);

                        for (const [key, value] of Object.entries(json.data)) {
                            if (key === "clientS") {
                                const clientStates = [
                                    "CLIENT_IDLE",
                                    "CLIENT_CONNECT",
                                    "CLIENT_CONNECTED"
                                ];
                                $("#" + key).val(clientStates[value] || `Unknown (${value})`);
                            } else if (key === "modemS") {
                                const modemStates = [
                                    "POWER_MODEM_ON",
                                    "POWER_MODEM_OFF",
                                    "WAIT_MODEM_ON",
                                    "MODEM_IDLE",
                                    "NETWORK_CONNECT",
                                    "NETWORK_CHECK",
                                    "GPRS_CONNECT",
                                    "GPRS_CHECK"
                                ];
                                $("#" + key).val(modemStates[value] || `Unknown (${value})`);
                            } else if (key === "rtcts") {
                                $('#' + key).val(value);
                                DataloggerApp.checkAndPromptUpdateTimestamp(value);
                            } else {
                                $("#" + key).val(value);
                            }
                        }
                        clearInterval(pollTimerGlobal);
                        pollTimerGlobal = setInterval(DataloggerApp.poll, scanRate);
                    } catch (e) {
                        console.log(e);
                    }

                }
        }
        },

       serverRequest: function(command, params) {
    return new Promise((resolve, reject) => {
        let url = esp32Server ? `/processCommand?command=${command}${params}` : `/${command}${params}`;
        console.log(url);
        $.ajax({
            url: url,
            type: "GET",
            timeout: 20000,
            success: function(data) {
                try {
                    let parsedData = JSON.parse(data);
                    let prettyJson = JSON.stringify(parsedData, null, 2); // Convert JSON to a pretty string with indentation
                    $('#log').append(prettyJson + "\r\n");
                } catch (err) {
                    $('#log').append(err + "\r\n");
                }
                resolve(data);
            },
            error: function(xhr, status, error) {
                reject(error);
            }
        });
    });
},

        // Update the requestMeasurementAndCreateDatatable function in the main app
        requestMeasurementAndCreateDatatable: async function(rowsToCollect) {
            
                return new Promise((resolve,reject)=>{
                    try {
                    console.log("Requesting server measurement");
                    let sensorType = $('#sensorType').val();
                    this.serverRequest("printjson=" + sensorType + ",," + rowsToCollect, "").then(jsonDataRaw =>{
                        let jsonData = JSON.parse(jsonDataRaw);
                        console.log(jsonData);
                        if (jsonData){ //&& jsonData.headers && jsonData.data) {
                            DataModule.createDataTable(jsonData);
                            resolve();
                        } 
                        
                    })
                } catch (e) {
                    reject (e);
                 }
                })
          
        },

        getConfig: function(source) {        
            /*
            let dummyconfig={
                "data":{
                "configSettings": {
                  "sampleInterval": 30,
                  "id":"310",
                  "sleepEnabled":false
                },
                "classInitializations": {
                  "rtc": {
                    "type": "RTC_DS3231"
                  },
                  "espRtc": {
                    "offset": 0
                  },
                  "wiper": {
                    "relay": 1
                  },
                  "modbus": {
                    "relay":1,
                    "sensorType":"OPUS",
                    "baud":9600,
                    "rx":19,
                    "tx":23
                  },
                  "sdi12": {
                    "relay":1,
                    "pin":18,
                    "addr":0
                  },
                  "gsmModem": {
                    "modemSerial": "SerialAT",
                    "clientIndex": 0,
                    "rx":26,
                    "tx":27
                  },
                  "battery": {
                    "method":"victron",
                    "pin": 5
                  },
                  "ioExpander": {
                    "address": "0x20"
                  }
                },
                "gsmConfiguration": {
                  "apn": "telstra.extranet",
                  "gprsUser": "",
                  "gprsPass": "",
                  "server": "aws-loggernet-server.ddns.net",
                  "port": 310,
                  "networkRetries": 0
                },
                "cameraConfiguration": {
                  "enabled": "false",
                  "relay": 2,
                  "onTime": 300000,
                  "interval": 900000
                },
                "systemStateFlags": {
                  "modemNetworkEnabled": true
                },
                "timingControl": {
                  "modemTurnsOffAtMinutes": 59
                },
                "WiFiSetup": {
                  "enabled": true,
                  "SSID":"ESP32_310",
                  "password":"ofbts84710",
                  "durationOn":3000000,
                  "port": 80,
                  "keepPoweredOn":true
                }
            }
              }
              $('#showConfig').removeAttr('disabled');
              $('#hideConfig').removeAttr('disabled');
              $('#updateConfig').removeAttr('disabled');
            document.getElementById('config-container').innerHTML = ConfigModule.createConfigUI(dummyconfig);
            */
            if(source == "server"){    
            this.serverRequest("getconfig", "").then(data => {
                config = JSON.parse(data);
                document.getElementById('config-container').innerHTML = ConfigModule.createConfigUI(config);
                $('#showConfig').removeAttr('disabled');
                $('#hideConfig').removeAttr('disabled');
                $('#updateConfig').removeAttr('disabled');
            }).catch(err => console.log("Error getting config:", err));
        } else if (source == "local"){
            document.getElementById('config-container').innerHTML = ConfigModule.createConfigUI(lastConfig);
        }
        },

        toggleConfigSettings: function(id, action) {
            const input = $(`#${id}`);
            const editBtn = input.next();
            const confirmBtn = editBtn.next();
            const cancelBtn = confirmBtn.next();

            switch(action) {
                case 'edit':
                    input.prop('disabled', false);
                    editBtn.prop('disabled', true);
                    confirmBtn.prop('disabled', false);
                    cancelBtn.prop('disabled', false);
                    break;
                case 'confirm':
                    input.prop('disabled', true);
                    editBtn.prop('disabled', false);
                    confirmBtn.prop('disabled', true);
                    cancelBtn.prop('disabled', true);
                    console.log(`Confirmed new value for ${id}: ${input.val()}`);
                    break;
                case 'cancel':
                    input.prop('disabled', true);
                    editBtn.prop('disabled', false);
                    confirmBtn.prop('disabled', true);
                    cancelBtn.prop('disabled', true);
                    console.log(`Cancelled edit for ${id}`);
                    break;
            }
        },

        prepend(value, array) {
            var newArray = array.slice();
            newArray.unshift(value);
            return newArray;
        },
    };
})();

// Initialize the app when the document is ready
$(document).ready(function() {
    DataloggerApp.init();
});