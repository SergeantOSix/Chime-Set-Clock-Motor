let ws = null;
let isConnected = false;
let reconnectAttempts = 0;
let maxReconnectAttempts = 10;
let reconnectDelay = 2000; // Initial delay in milliseconds

// Time prediction variables - separate for each clock
let lastUpdateTime = null;
let lastTimeValues = null;
let faceClockLastUpdateTime = null;
let faceClockLastTimeValues = null;
let timePredictionInterval = null;
let clockRate = 1; // Default rate multiplier

// Connect to WebSocket
function connectWebSocket() {
    ws = new WebSocket("ws://" + window.location.hostname + "/ws");
    
    ws.onopen = function() {
        isConnected = true;
        reconnectAttempts = 0;
        updateStatus(true);
        sendPageRequest();
        
            // Start time prediction interval
            startTimePrediction();
        };
    
        ws.onmessage = function(event) {
            try {
                let data = JSON.parse(event.data);
                handleMessage(data);
            } catch (e) {
                console.error("Error parsing message:", e);
            }
        };
    
        ws.onclose = function() {
            isConnected = false;
            updateStatus(false);
            stopTimePrediction();
        
            // Attempt to reconnect if we haven't exceeded max attempts
            if (reconnectAttempts < maxReconnectAttempts) {
                reconnectAttempts++;
            
                // Exponential backoff: increase delay with each attempt
                reconnectDelay = Math.min(reconnectDelay * 2, 30000); // Max 30 seconds
            
                setTimeout(connectWebSocket, reconnectDelay);
            }
        };
    
    ws.onerror = function(error) {
        console.error("WebSocket Error:", error);
    };
}

// Handle incoming messages
function handleMessage(data) {
    //Debug code
    //console.log("Raw message received:", data);

    // Handle new protocol format with timeClock and faceClock objects
    if ("timeClock" in data || "faceClock" in data) {
        handleNewProtocol(data);
        return;
    }
    
    // Handle clock JSON format (old format) - check for time-related fields
    if ("hour" in data || "minute" in data || "second" in data || "Hour" in data || "Minute" in data || "Second" in data || "minutes" in data || "Minutes" in data) {
        updateClockFromJSON(data);
        return;
    }
    
    // Handle time object format (old format)
    if ("time" in data) {
        updateTime(data.time);
    }
    // Handle clock object format
    if ("clock" in data) {
        updateClockFromJSON(data.clock);
    }
    if ("date" in data) {
        updateDate(data.date);
    }
    if ("status" in data) {
        updateMotorStatus(data.status);
    }
    if ("connection" in data) {
        updateConnectionStatus(data.connection);
    }
}

// Handle new protocol format with timeClock and faceClock
function handleNewProtocol(data) {
    const timeClockData = data.timeClock || data.TimeClock;
    const faceClockData = data.faceClock || data.FaceClock;
    
    if (timeClockData) {
                updateClockFromJSON(timeClockData, 'timeClock');
                // Store last update time and values for prediction
                lastUpdateTime = new Date();
                lastTimeValues = {
                    hour: timeClockData.hour,
                    minutes: timeClockData.minutes,
                    seconds: timeClockData.seconds,
                    milliseconds: timeClockData.milliseconds,
                    microseconds: timeClockData.microseconds,
                    rate: timeClockData.rate !== undefined ? timeClockData.rate : 1
                };
                // DEBUG: console.log('TimeClock updated - Rate:', lastTimeValues.rate);
            }
    
        if (faceClockData) {
                updateClockFromJSON(faceClockData, 'faceClock');
                // Store separate last update time and values for faceClock prediction
                faceClockLastUpdateTime = new Date();
                faceClockLastTimeValues = {
                    hour: faceClockData.hour,
                    minutes: faceClockData.minutes,
                    seconds: faceClockData.seconds,
                    milliseconds: faceClockData.milliseconds,
                    microseconds: faceClockData.microseconds,
                    rate: faceClockData.rate !== undefined ? faceClockData.rate : 1
                };
                // DEBUG: console.log('FaceClock updated - Rate:', faceClockLastTimeValues.rate);
            }
        }

// Start time prediction interval
function startTimePrediction() {
    if (timePredictionInterval) {
        clearInterval(timePredictionInterval);
    }
    
    timePredictionInterval = setInterval(function() {
        // Only update if rate is not zero
        if (isConnected && lastTimeValues && lastTimeValues.rate !== 0) {
            updatePredictedClock();
        }
        if (isConnected && faceClockLastTimeValues && faceClockLastTimeValues.rate !== 0) {
            updatePredictedFaceClock();
        }
    }, 50); // Update every 50ms
}

// Stop time prediction interval
function stopTimePrediction() {
    if (timePredictionInterval) {
        clearInterval(timePredictionInterval);
        timePredictionInterval = null;
    }
}

// Update predicted clock based on last known values
function updatePredictedClock() {
    if (!lastTimeValues) return;
    
    // If rate is zero, don't update the clock (it's frozen)
    if (lastTimeValues.rate === 0) return;
    
    const now = new Date();
    const elapsedMs = (now.getTime() - lastUpdateTime.getTime()) * lastTimeValues.rate;
    
    // Calculate predicted time based on elapsed time with rate multiplier
    const predicted = {
        hour: lastTimeValues.hour,
        minutes: lastTimeValues.minutes,
        seconds: lastTimeValues.seconds,
        milliseconds: Math.floor(elapsedMs % 1000),
        microseconds: 0   // Don't predict microseconds
    };
    
    // Calculate total elapsed seconds (use integer division to avoid floating point issues)
    const totalSeconds = Math.floor(elapsedMs / 1000);
    
    // Add elapsed seconds to current time
    let newSeconds = predicted.seconds + totalSeconds;
    let carry = Math.floor(newSeconds / 60);
    predicted.seconds = newSeconds % 60;
    predicted.minutes += carry;
    
    // Handle minute rollover
    carry = Math.floor(predicted.minutes / 60);
    predicted.minutes = predicted.minutes % 60;
    predicted.hour += carry;
    
    // Handle hour rollover (12-hour format for analog clock)
    carry = Math.floor(predicted.hour / 24);
    predicted.hour = predicted.hour % 24;
    // Convert to 12-hour format
    if (predicted.hour === 0) {
        predicted.hour = 12;
    } else if (predicted.hour > 12) {
        predicted.hour = predicted.hour - 12;
    }
    
    // Update display
    updateClockFromJSON(predicted, 'timeClock');
}

// Update predicted face clock based on last known values
function updatePredictedFaceClock() {
    if (!faceClockLastTimeValues) return;
    
    // If rate is zero, don't update the clock (it's frozen)
    if (faceClockLastTimeValues.rate === 0) return;
    
    const now = new Date();
    const elapsedMs = (now.getTime() - faceClockLastUpdateTime.getTime()) * faceClockLastTimeValues.rate;
    
    // Calculate predicted time based on elapsed time with rate multiplier
    const predicted = {
        hour: faceClockLastTimeValues.hour,
        minutes: faceClockLastTimeValues.minutes,
        seconds: faceClockLastTimeValues.seconds,
        milliseconds: Math.floor(elapsedMs % 1000),
        microseconds: 0   // Don't predict microseconds
    };
    
    // Calculate total elapsed seconds (use integer division to avoid floating point issues)
    const totalSeconds = Math.floor(elapsedMs / 1000);
    
    // Add elapsed seconds to current time
    let newSeconds = predicted.seconds + totalSeconds;
    let carry = Math.floor(newSeconds / 60);
    predicted.seconds = newSeconds % 60;
    predicted.minutes += carry;
    
    // Handle minute rollover
    carry = Math.floor(predicted.minutes / 60);
    predicted.minutes = predicted.minutes % 60;
    predicted.hour += carry;
    
    // Handle hour rollover (12-hour format for analog clock)
    carry = Math.floor(predicted.hour / 24);
    predicted.hour = predicted.hour % 24;
    // Convert to 12-hour format
    if (predicted.hour === 0) {
        predicted.hour = 12;
    } else if (predicted.hour > 12) {
        predicted.hour = predicted.hour - 12;
    }
    
    // Update display
    updateClockFromJSON(predicted, 'faceClock');
}

// Update clock from JSON object
function updateClockFromJSON(clockData, clockId) {
    // Handle different possible field names - try multiple variations
    // Firmware sends "hour" (singular), JavaScript expects "hours" (plural)
    const hour = clockData.hour !== undefined ? clockData.hour : clockData.hours || clockData.Hour || clockData.Hours || 0;
    const minutes = clockData.minutes !== undefined ? clockData.minutes : clockData.Minutes || clockData.minute || clockData.Minute || clockData.minuts || clockData.Minuts || 0;
    const seconds = clockData.seconds !== undefined ? clockData.seconds : clockData.Seconds || clockData.second || clockData.Second || 0;
    const milliseconds = clockData.milliseconds !== undefined ? clockData.milliseconds : clockData.Milliseconds || clockData.Millisecond || 0;
    const microseconds = clockData.microseconds !== undefined ? clockData.microseconds : clockData.Microseconds || clockData.Microsecond || 0;
    
    // Convert to 12-hour format for analog clock display
    let displayHour = hour;
    if (displayHour === 0) {
        displayHour = 12;
    } else if (displayHour > 12) {
        displayHour = displayHour - 12;
    }
    
    const hours = String(displayHour).padStart(2, '0');
    const mins = String(minutes).padStart(2, '0');
    const secs = String(seconds).padStart(2, '0');
    const ms = String(milliseconds).padStart(3, '0');
    const us = String(microseconds).padStart(6, '0');
    
    const timeDisplay = document.getElementById(clockId || 'timeDisplay');
    const dateDisplay = document.getElementById('dateDisplay');
    
    if (timeDisplay) {
        // Display format: HH:MM:SS.mmm (milliseconds only, always 3 decimal places)
        // Truncate to 3 decimal places
        const msDisplay = String(milliseconds).slice(0, 3).padEnd(3, '0');
        const displayTime = `${hours}:${mins}:${secs}.${msDisplay}`;
        timeDisplay.textContent = displayTime;
        
        // Hide loading text and show date when time is displayed (only for timeClock)
        if (clockId === 'timeClock' && dateDisplay) {
            dateDisplay.textContent = '';
            dateDisplay.style.display = 'block';
        }
    }
}

// Update date display
function updateDate(date) {
    const days = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
    const months = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
    document.getElementById('dateDisplay').textContent = `${days[date.day]} ${date.month} ${date.dayOfMonth}, ${date.year}`;
}

// Update motor status
function updateMotorStatus(status) {
    document.getElementById('motorStatus').textContent = status;
}

// Update connection status
function updateConnectionStatus(connected) {
    const statusEl = document.getElementById('statusDisplay');
    const connStatusEl = document.getElementById('connectionStatus');
    
    if (connected) {
        statusEl.className = 'status connected';
        statusEl.textContent = 'Connected';
        connStatusEl.textContent = 'Connected';
    } else {
        statusEl.className = 'status disconnected';
        statusEl.textContent = 'Disconnected';
        connStatusEl.textContent = 'Disconnected';
    }
}

// Update status display
function updateStatus(connected) {
    const statusEl = document.getElementById('statusDisplay');
    if (connected) {
        statusEl.className = 'status connected';
        statusEl.textContent = 'Connected';
    } else {
        statusEl.className = 'status disconnected';
        statusEl.textContent = 'Disconnected';
    }
}

// Send command to ESP32
function sendCommand(command) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        const message = { command: command };
        ws.send(JSON.stringify(message));
    } else {
        alert("Not connected to ESP32. Please check WiFi settings.");
    }
}

// Send set time command to ESP32
function sendSetTime(hours, minutes, seconds, milliseconds, microseconds) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        const message = {
            command: "setTime",
            currentTime: {
                hours: parseInt(hours),
                minutes: parseInt(minutes),
                seconds: parseInt(seconds),
                milliseconds: parseInt(milliseconds),
                microseconds: parseInt(microseconds)
            }
        };
        ws.send(JSON.stringify(message));
    } else {
        alert("Not connected to ESP32. Please check WiFi settings.");
    }
}

// Send set clock face command to ESP32
function sendSetClockFace(hours, minutes, seconds, milliseconds, microseconds) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        const message = {
            command: "setClockFace",
            currentTime: {
                hours: parseInt(hours),
                minutes: parseInt(minutes),
                seconds: parseInt(seconds),
                milliseconds: parseInt(milliseconds),
                microseconds: parseInt(microseconds)
            }
        };
        ws.send(JSON.stringify(message));
    } else {
        alert("Not connected to ESP32. Please check WiFi settings.");
    }
}

// Send page request to ESP32
function sendPageRequest() {
    if (ws && ws.readyState === WebSocket.OPEN) {
        const message = { page: "home" };
        ws.send(JSON.stringify(message));
    }
}

// Navigate to different pages
function navigateTo(page) {
    window.location.href = '/' + page;
}

// Update last update timestamp
function updateLastUpdate() {
    const now = new Date();
    document.getElementById('lastUpdate').textContent = now.toLocaleTimeString();
}

// Initialize WebSocket on page load
window.onload = function() {
    connectWebSocket();
    
    // Update last update time every minute
    setInterval(updateLastUpdate, 60000);
    
    // Request current time on connection
    setTimeout(sendPageRequest, 1000);
};

// Cleanup on page unload
window.onbeforeunload = function() {
    stopTimePrediction();
    if (ws) {
        ws.close();
    }
};

// Open set time dialog
function openSetTimeDialog() {
    const dialog = document.getElementById('setTimeDialog');
    if (dialog) {
        dialog.style.display = 'flex';
    }
}

// Close set time dialog
function closeSetTimeDialog() {
    const dialog = document.getElementById('setTimeDialog');
    if (dialog) {
        dialog.style.display = 'none';
    }
}

// Confirm set time
function confirmSetTime() {
    const hours = document.getElementById('setHours').value;
    const minutes = document.getElementById('setMinutes').value;
    const seconds = document.getElementById('setSeconds').value;
    const milliseconds = document.getElementById('setMilliseconds').value;
    const microseconds = document.getElementById('setMicroseconds').value;
    
    sendSetTime(hours, minutes, seconds, milliseconds, microseconds);
    closeSetTimeDialog();
}

// Open set clock face dialog
function openSetClockFaceDialog() {
    const dialog = document.getElementById('setClockFaceDialog');
    if (dialog) {
        dialog.style.display = 'flex';
    }
}

// Close set clock face dialog
function closeSetClockFaceDialog() {
    const dialog = document.getElementById('setClockFaceDialog');
    if (dialog) {
        dialog.style.display = 'none';
    }
}

// Confirm set clock face
function confirmSetClockFace() {
    const hours = document.getElementById('faceHours').value;
    const minutes = document.getElementById('faceMinutes').value;
    const seconds = document.getElementById('faceSeconds').value;
    const milliseconds = document.getElementById('faceMilliseconds').value;
    const microseconds = document.getElementById('faceMicroseconds').value;
    
    sendSetClockFace(hours, minutes, seconds, milliseconds, microseconds);
    closeSetClockFaceDialog();
}

// Close dialog when clicking outside
window.onclick = function(event) {
    const dialog = document.getElementById('setTimeDialog');
    if (event.target === dialog) {
        closeSetTimeDialog();
    }
};