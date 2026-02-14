// 1. User Database
const users = [
    {
        username: "Kasun2001",
        password: "kwarun23",
        name: "Kasun Warunathilaka",
        rfid: "09047B05",
        balance: 68296, 
        pin: "6756"
    },
    {
        username: "NImal20@1",
        password: "gNthilaka21",
        name: "Nimal Gunathilake",
        rfid: "9C77A904",
        balance: 10820,
        pin: "2698"
    }
];

let currentUser = null;

// 2. Select Elements
const loginSection = document.getElementById("login-section");
const dashboardSection = document.getElementById("dashboard-section");
const paymentSection = document.getElementById("payment-section");

const loginForm = document.getElementById("login-form");
const logoutBtn = document.getElementById("logout-btn");
const topupBtn = document.getElementById("topup-btn");
const cancelPaymentBtn = document.getElementById("cancel-payment-btn");
const paymentForm = document.getElementById("payment-form");
const errorMsg = document.getElementById("error-msg");

// Dashboard Fields
const displayName = document.getElementById("display-name");
const resName = document.getElementById("res-name");
const resRfid = document.getElementById("res-rfid");
const resBalance = document.getElementById("res-balance");
const resPin = document.getElementById("res-pin");

// 3. Login Logic
loginForm.addEventListener("submit", function(event) {
    event.preventDefault();
    const usernameInput = document.getElementById("username").value;
    const passwordInput = document.getElementById("password").value;

    const userFound = users.find(user => 
        user.username === usernameInput && user.password === passwordInput
    );

    if (userFound) {
        currentUser = userFound;
        loadDashboard();
    } else {
        errorMsg.textContent = "Invalid Username or Password";
    }
});

// 4. Load Dashboard
function loadDashboard() {
    loginSection.classList.add("hidden");
    paymentSection.classList.add("hidden");
    dashboardSection.classList.remove("hidden");

    displayName.textContent = currentUser.name.split(" ")[0];
    resName.textContent = currentUser.name;
    resRfid.textContent = currentUser.rfid;
    resBalance.textContent = currentUser.balance.toLocaleString();
    resPin.textContent = currentUser.pin;
    
    errorMsg.textContent = "";
}

// 5. Navigation
topupBtn.addEventListener("click", function() {
    dashboardSection.classList.add("hidden");
    paymentSection.classList.remove("hidden");
});

cancelPaymentBtn.addEventListener("click", function() {
    paymentForm.reset();
    loadDashboard();
});

// 6. Payment Logic
paymentForm.addEventListener("submit", function(event) {
    event.preventDefault();
    const amountInput = document.getElementById("topup-amount").value;
    const amountToAdd = parseFloat(amountInput);

    if (amountToAdd > 0) {
        currentUser.balance += amountToAdd;
        alert(`Payment Successful! Added ${amountToAdd} to balance.`);
        paymentForm.reset();
        loadDashboard();
    } else {
        alert("Please enter a valid amount.");
    }
});

// 7. Logout
logoutBtn.addEventListener("click", function() {
    currentUser = null;
    loginForm.reset();
    dashboardSection.classList.add("hidden");
    loginSection.classList.remove("hidden");
});