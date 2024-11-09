


// Modularized JavaScript
const DataloggerApp = (function () {
    // Global variables
    let scanRate = 5000;
    let maximumTimeOnline = 300000;
    let timeoutClock = maximumTimeOnline;
    let timeoutReached = false;
    let pollTimerGlobal;
    let toPoll = true;
    let esp32Server = true; // True if being hosted on the ESP32 and accessed using WiFi

    // Main App Functions
    return {
        init: function () {
            this.bindEvents();
            $('#scanRate').val(scanRate);
            $('#timeoutValue').html(0);
            pollTimerGlobal = setInterval(this.poll, scanRate);
        },

        bindEvents: function () {
            $('#toPoll').on('change', () => {
                let booltext = $('#toPoll option:selected').text();
                console.log(booltext)
                if (booltext == "True") {
                    toPoll = true;
                } else {
                    toPoll = false;
                }
            })
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
                                    "MODEM_OFF",
                                    "MODEM_HANG"
                                ];
                                $("#" + key).val(modemStates[value] || `Unknown (${value})`);
                            } else if (key === "networkS") {
                                const networkStates = [
                                    "NETWORK_CONNECT",
                                    "NETWORK_CHECK",
                                    "GPRS_CONNECT",
                                    "GPRS_CHECK",
                                    "NETWORK_CONNECTED",
                                    "NETWORK_FAILED",
                                    "NETWORK_HANG"
                                ];
                                $("#" + key).val(networkStates[value] || `Unknown (${value})`);
                            } else if (key === "rtcts") {
                                $('#' + key).val(value);
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
                        } catch (err) {
                            console.log(err);
                        }
                        resolve(data);
                    },
                    error: function (xhr, status, error) {
                        reject(error);
                    }
                });
            });
        },
    };
})();

// Initialize the app when the document is ready
$(document).ready(function () {
    DataloggerApp.init();
});


