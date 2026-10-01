let ws = null;
let isConnected = false;
let reconnectAttempts = 0;
let maxReconnectAttempts = 10;
let reconnectDelay = 2000; // Initial delay in milliseconds

// Connect to WebSocket
function connectWebSocket() {
    ws = new WebSocket("ws://" + window.location.hostname + "/ws");
    
    ws.onopen = function() {
        isConnected = true;
        reconnectAttempts = 0;
        updateStatus(true);
        sendPageRequest();
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
    // Handle clock JSON format (new format) - check for time-related fields
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

// Update clock from JSON object
function updateClockFromJSON(clockData) {
    // Handle different possible field names - try multiple variations
    const hour = clockData.hour !== undefined ? clockData.hour : clockData.Hour || clockData.hour || 0;
    const minutes = clockData.minutes !== undefined ? clockData.minutes : clockData.Minutes || clockData.minute || clockData.Minute || clockData.minutes || clockData.Minutes || clockData.minutes || 0;
    const seconds = clockData.seconds !== undefined ? clockData.seconds : clockData.Seconds || clockData.second || clockData.Second || clockData.seconds || 0;
    const milliseconds = clockData.milliseconds !== undefined ? clockData.milliseconds : clockData.Milliseconds || clockData.millisecond || clockData.Millisecond || clockData.milliseconds || 0;
    const microseconds = clockData.microseconds !== undefined ? clockData.microseconds : clockData.Microseconds || clockData.microsecond || clockData.Microsecond || clockData.microseconds || 0;
    
    const hours = String(hour).padStart(2, '0');
    const mins = String(minutes).padStart(2, '0');
    const secs = String(seconds).padStart(2, '0');
    const ms = String(milliseconds).padStart(3, '0');
    const us = String(microseconds).padStart(6, '0');
    
    const timeDisplay = document.getElementById('timeDisplay');
    const dateDisplay = document.getElementById('dateDisplay');
    
    if (timeDisplay) {
        // Display format: HH:MM:SS.mmmmmm (milliseconds + microseconds combined)
        // Only show the last 6 digits of the combined milliseconds+microseconds
        const combined = String(milliseconds) + String(microseconds);
        const displayTime = `${hours}:${mins}:${secs}.${combined.slice(-6)}`;
        timeDisplay.textContent = displayTime;
        
        // Hide loading text and show date when time is displayed
        if (dateDisplay) {
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

// Close dialog when clicking outside
window.onclick = function(event) {
    const dialog = document.getElementById('setTimeDialog');
    if (event.target === dialog) {
        closeSetTimeDialog();
    }
};