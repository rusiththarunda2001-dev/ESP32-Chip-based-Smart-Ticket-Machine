// ====================================================================
// CONFIGURATION
// ====================================================================
const ADMIN_APP_SCRIPT_URL = 'https://script.google.com/macros/s/AKfycbwmgevL04jLPEGa7tW4CUGW2wRuVCwxYecfJmUrSp_RA4tcLiKHenOKmS54sN4fmZUp/exec'; 

// Global variables
let currentPassenger = null; 
let currentMachine = null;
let machineStationsData = [];

// --- DOM ELEMENT REFERENCES ---
const pName = document.getElementById('p-name');
const pRfid = document.getElementById('p-rfid');
const pPin = document.getElementById('p-pin');
const pStatus = document.getElementById('p-status');
const pBalance = document.getElementById('p-balance');


// ====================================================================
// 1. NAVIGATION & PAGE LOADING
// ====================================================================

function showPage(pageId) {
    // Hide all pages
    document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
    document.querySelectorAll('.page').forEach(p => p.classList.add('hidden'));
    
    // Show the active page
    const activePage = document.getElementById(pageId);
    activePage.classList.remove('hidden');
    activePage.classList.add('active');

    // Clear forms when leaving pages
    if (pageId !== 'passenger-page') clearPassengerForm();
    if (pageId !== 'ticket-machine-page') clearMachineForm();
}

function clearPassengerForm() {
    pName.value = '';
    pRfid.value = '';
    pPin.value = '';
    pStatus.value = 'Active'; 
    pBalance.value = '';
    document.getElementById('passenger-search').value = '';
    currentPassenger = null;
}

function clearMachineForm() {
    document.getElementById('machine-search').value = '';
    document.getElementById('m-station').value = '';
    const tbody = document.getElementById('machine-stations-body');
    if (tbody) tbody.innerHTML = '';
    currentMachine = null;
    machineStationsData = [];
}


// ====================================================================
// 2. PASSENGER MANAGEMENT
// ====================================================================

async function searchPassenger() {
    const searchInput = document.getElementById('passenger-search').value.trim();
    if (!searchInput) return alert("Please enter a Name or RFID to search.");
    
    clearPassengerForm(); 

    const MACHINE_PLACEHOLDER = 'model001'; 
    const encodedMachineName = encodeURIComponent(MACHINE_PLACEHOLDER);
    const url = `${ADMIN_APP_SCRIPT_URL}?rfid=${searchInput}&machine=${encodedMachineName}`; 

    try {
        const response = await fetch(url);
        const data = await response.json();

        if (data.error || !data.passenger) {
            return alert(data.error || "Passenger not found. Check if the RFID is correct.");
        }

        const p = data.passenger;
        
        currentPassenger = {
            rfid: p.rfid, 
            name: p.name,
            pin: p.pin,
            status: p.status,
            balance: p.balance 
        };
        
        pName.value = currentPassenger.name;
        pRfid.value = currentPassenger.rfid;
        pPin.value = currentPassenger.pin;
        pStatus.value = currentPassenger.status;
        pBalance.value = parseFloat(currentPassenger.balance).toFixed(2);
        
        alert(`Passenger ${currentPassenger.name} loaded successfully!`);

    } catch (error) {
        console.error('Fetch error:', error);
        alert("A network error occurred while connecting to the server. Check the App Script URL and deployment.");
    }
}

async function updatePassenger() {
    if (!currentPassenger) {
        return alert("Please search and load a passenger record first.");
    }
    
    const newBalance = parseFloat(pBalance.value);
    if (isNaN(newBalance) || newBalance < 0) {
        return alert("Invalid balance amount.");
    }

    const payload = {
        rfid: currentPassenger.rfid, 
        name: pName.value.trim(),
        pin: pPin.value.trim(),
        status: pStatus.value,
        balance: newBalance 
    };

    try {
        const response = await fetch(ADMIN_APP_SCRIPT_URL, {
            method: 'POST',
            body: JSON.stringify(payload), 
            headers: {
                'Content-Type': 'text/plain'
            }
        });

        const textResponse = await response.text();
        
        if (textResponse.includes("successfully")) {
            currentPassenger.name = payload.name;
            currentPassenger.pin = payload.pin;
            currentPassenger.status = payload.status;
            currentPassenger.balance = payload.balance;
            alert(`Record for ${payload.name} updated successfully!`);
        } else {
            alert(`Update Failed: ${textResponse}`);
        }

    } catch (error) {
        console.error('Update fetch error:', error);
        alert("A network error occurred while updating the record.");
    }
}


// ====================================================================
// 3. TICKET MACHINE MANAGEMENT
// ====================================================================

async function searchMachine() {
    const modelId = document.getElementById('machine-search').value.trim();
    if (!modelId) return alert("Please enter a Model ID to search.");
    
    clearMachineForm();

    const url = `${ADMIN_APP_SCRIPT_URL}?action=getMachine&modelId=${encodeURIComponent(modelId)}`; 

    try {
        const response = await fetch(url);
        const data = await response.json();

        if (data.error || !data.machine) {
            return alert(data.error || "Machine not found. Check if the Model ID is correct.");
        }

        const machine = data.machine;
        
        // Store machine data globally
        currentMachine = {
            modelId: modelId,
            assignedStation: machine.assignedStation,
            stations: machine.stations
        };

        // Update the form
        document.getElementById('m-station').value = currentMachine.assignedStation;
        
        // Store stations data for editing
        machineStationsData = [...currentMachine.stations];
        
        // Render the table
        renderMachineStations();
        
        alert(`Machine ${modelId} loaded successfully!`);

    } catch (error) {
        console.error('Fetch error:', error);
        alert("A network error occurred while connecting to the server. Check the App Script URL and deployment.");
    }
}

function renderMachineStations() {
    const tbody = document.getElementById('machine-stations-body');
    if (!tbody) return;

    tbody.innerHTML = '';
    
    machineStationsData.forEach((station, index) => {
        const row = document.createElement('tr');
        row.draggable = true;
        row.dataset.index = index;
        row.className = 'draggable-row';
        
        row.innerHTML = `
            <td class="drag-handle">
                <span class="drag-icon">☰</span>
                <input type="text" value="${station.name}" onchange="updateStationName(${index}, this.value)" class="station-name-input" placeholder="Station Name">
            </td>
            <td><input type="number" value="${station.class1}" onchange="updateStationPrice(${index}, 'class1', this.value)" class="price-input"></td>
            <td><input type="number" value="${station.class2}" onchange="updateStationPrice(${index}, 'class2', this.value)" class="price-input"></td>
            <td><input type="number" value="${station.class3}" onchange="updateStationPrice(${index}, 'class3', this.value)" class="price-input"></td>
            <td><button class="delete-btn" onclick="deleteStation(${index})">🗑️ DELETE</button></td>
        `;
        
        // Add drag event listeners
        row.addEventListener('dragstart', handleDragStart);
        row.addEventListener('dragover', handleDragOver);
        row.addEventListener('drop', handleDrop);
        row.addEventListener('dragend', handleDragEnd);
        
        tbody.appendChild(row);
    });
}

function updateStationName(index, value) {
    const newName = value.trim();
    if (!newName) {
        alert("Station name cannot be empty!");
        renderMachineStations(); // Re-render to restore old value
        return;
    }
    
    // Check if the new name already exists (excluding current station)
    const exists = machineStationsData.some((s, i) => 
        i !== index && s.name.toLowerCase() === newName.toLowerCase()
    );
    
    if (exists) {
        alert("A station with this name already exists!");
        renderMachineStations(); // Re-render to restore old value
        return;
    }
    
    machineStationsData[index].name = newName;
}

// Drag and Drop Handlers
let draggedIndex = null;

function handleDragStart(e) {
    draggedIndex = parseInt(this.dataset.index);
    this.style.opacity = '0.4';
    e.dataTransfer.effectAllowed = 'move';
}

function handleDragOver(e) {
    if (e.preventDefault) {
        e.preventDefault();
    }
    e.dataTransfer.dropEffect = 'move';
    
    // Visual feedback
    this.style.borderTop = '3px solid #4a90e2';
    return false;
}

function handleDrop(e) {
    if (e.stopPropagation) {
        e.stopPropagation();
    }
    
    const dropIndex = parseInt(this.dataset.index);
    
    if (draggedIndex !== dropIndex) {
        // Remove the dragged item
        const draggedItem = machineStationsData[draggedIndex];
        machineStationsData.splice(draggedIndex, 1);
        
        // Insert at new position
        const newIndex = draggedIndex < dropIndex ? dropIndex - 1 : dropIndex;
        machineStationsData.splice(newIndex, 0, draggedItem);
        
        // Re-render the table
        renderMachineStations();
        
        alert('Station reordered. Click SAVE CHANGES to update the sheet.');
    }
    
    return false;
}

function handleDragEnd(e) {
    this.style.opacity = '1';
    
    // Remove all border highlights
    document.querySelectorAll('.draggable-row').forEach(row => {
        row.style.borderTop = '';
    });
}

function updateStationPrice(index, classType, value) {
    const price = parseFloat(value);
    if (!isNaN(price) && price >= 0) {
        machineStationsData[index][classType] = price;
    }
}

function deleteStation(index) {
    if (!confirm(`Are you sure you want to delete "${machineStationsData[index].name}"?`)) {
        return;
    }
    
    // Remove the station from the array
    machineStationsData.splice(index, 1);
    
    // Re-render the table
    renderMachineStations();
    
    alert("Station deleted. Click SAVE CHANGES to update the sheet.");
}

function addNewStation() {
    if (!currentMachine) {
        return alert("Please load a machine first.");
    }

    const stationName = prompt("Enter new station name:");
    if (!stationName || stationName.trim() === '') {
        return alert("Station name cannot be empty.");
    }

    // Check if station already exists
    const exists = machineStationsData.some(s => s.name.toLowerCase() === stationName.trim().toLowerCase());
    if (exists) {
        return alert("This station already exists!");
    }

    // Add new station with default prices
    machineStationsData.push({
        name: stationName.trim(),
        class1: 0,
        class2: 0,
        class3: 0
    });

    // Re-render the table
    renderMachineStations();
    
    alert(`Station "${stationName}" added. Set the prices and click SAVE CHANGES.`);
}

async function saveMachineChanges() {
    if (!currentMachine) {
        return alert("Please load a machine first.");
    }

    const assignedStation = document.getElementById('m-station').value.trim();
    if (!assignedStation) {
        return alert("Please enter an assigned station name.");
    }

    const payload = {
        action: 'updateMachine',
        modelId: currentMachine.modelId,
        assignedStation: assignedStation,
        stations: machineStationsData
    };

    try {
        const response = await fetch(ADMIN_APP_SCRIPT_URL, {
            method: 'POST',
            body: JSON.stringify(payload), 
            headers: {
                'Content-Type': 'text/plain'
            }
        });

        const textResponse = await response.text();
        
        if (textResponse.includes("successfully")) {
            currentMachine.assignedStation = assignedStation;
            currentMachine.stations = [...machineStationsData];
            alert(`Machine ${currentMachine.modelId} updated successfully!`);
        } else {
            alert(`Update Failed: ${textResponse}`);
        }

    } catch (error) {
        console.error('Update fetch error:', error);
        alert("A network error occurred while updating the machine.");
    }
}


// ====================================================================
// 4. TRANSACTION HISTORY
// ====================================================================

async function filterTransactions() {
    const modelId = document.getElementById('t-model-id').value.trim();
    const filterDate = document.getElementById('t-date').value;
    
    if (!modelId) {
        return alert("Please enter a Model ID to load transactions.");
    }

    const url = `${ADMIN_APP_SCRIPT_URL}?action=getTransactions&modelId=${encodeURIComponent(modelId)}`;

    try {
        const response = await fetch(url);
        const data = await response.json();

        if (data.error) {
            alert(data.error);
            const tbody = document.getElementById('transaction-body');
            if (tbody) {
                tbody.innerHTML = '<tr><td colspan="4">No transactions found.</td></tr>';
            }
            return;
        }

        let transactions = data.transactions || [];
        
        // Debug: Log all transactions with dates
        console.log("Total transactions loaded:", transactions.length);
        console.log("Filter date:", filterDate);
        
        // Filter by date if provided
        if (filterDate) {
            console.log("Filtering by date:", filterDate);
            transactions.forEach(t => {
                console.log("Transaction date:", t.date, "Match:", t.date === filterDate);
            });
            
            transactions = transactions.filter(t => t.date === filterDate);
            console.log("Filtered transactions count:", transactions.length);
        }

        displayTransactions(transactions);
        
        if (transactions.length === 0) {
            if (filterDate) {
                alert(`No transactions found for ${modelId} on ${filterDate}`);
            } else {
                alert(`No transactions found for ${modelId}`);
            }
        } else {
            if (filterDate) {
                alert(`Found ${transactions.length} transactions for ${modelId} on ${filterDate}`);
            } else {
                alert(`Loaded ${transactions.length} transactions for ${modelId}`);
            }
        }

    } catch (error) {
        console.error('Fetch error:', error);
        alert("A network error occurred while loading transactions.");
    }
}

function displayTransactions(transactions) {
    const tbody = document.getElementById('transaction-body');
    if (!tbody) return;

    tbody.innerHTML = '';

    if (transactions.length === 0) {
        tbody.innerHTML = '<tr><td colspan="4">No transactions to display.</td></tr>';
        return;
    }

    transactions.forEach(transaction => {
        const row = document.createElement('tr');
        row.innerHTML = `
            <td>${transaction.name}</td>
            <td>${transaction.time}</td>
            <td>${transaction.rfid}</td>
            <td>LKR ${parseFloat(transaction.price).toFixed(2)}</td>
        `;
        tbody.appendChild(row);
    });
}

// End of file