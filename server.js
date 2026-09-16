const express = require('express');
const multer = require('multer');
const path = require('path');
const fs = require('fs');
const cors = require('cors');

const app = express();
const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.static('public'));

// --- LONG-POLLING & ACKNOWLEDGMENT ---
let waitingClients = [];
let esp32Connected = false;
let lastDownloadStatus = "Idle"; // Naya variable status track karne ke liye

app.get('/poll', (req, res) => {
    console.log('[POLL] ESP32 Connected for Polling!');
    esp32Connected = true;
    lastDownloadStatus = "ESP32 is Ready & Waiting...";
    
    req.setTimeout(50000, () => {
        esp32Connected = false;
        res.status(204).end();
    });

    waitingClients.push(res);

    req.on('close', () => {
        waitingClients = waitingClients.filter(client => client !== res);
        esp32Connected = false;
    });
});

// ESP32 yahan batayega ki file save hui ya fail hui
app.get('/ack', (req, res) => {
    const status = req.query.msg || "Unknown";
    console.log(`[ACK] ESP32 reported: ${status}`);
    lastDownloadStatus = status;
    res.send("OK");
});

app.get('/status', (req, res) => {
    res.json({ 
        connected_devices: esp32Connected ? 1 : 0,
        download_status: lastDownloadStatus 
    });
});
// --------------------------------------------------

const storage = multer.diskStorage({
    destination: (req, file, cb) => {
        const uploadPath = path.join(__dirname, 'public', 'uploads'); 
        if (!fs.existsSync(uploadPath)) fs.mkdirSync(uploadPath, { recursive: true });
        cb(null, uploadPath);
    },
    filename: (req, file, cb) => {
        cb(null, file.originalname); 
    }
});
const upload = multer({ storage: storage });

app.post('/upload', upload.single('ota_file'), (req, res) => {
    if (!req.file) return res.status(400).json({ error: 'No file uploaded.' });

    console.log(`[SERVER] File received: ${req.file.originalname}. Sending to ESP32...`);
    lastDownloadStatus = "Downloading to ESP32... Please wait!";

    if (waitingClients.length > 0) {
        waitingClients.forEach(client => {
            client.download(req.file.path, req.file.originalname, (err) => {
                if (err) {
                    console.error("Error sending file:", err);
                    lastDownloadStatus = "Error during transfer!";
                }
            });
        });
        waitingClients = [];
    } else {
        lastDownloadStatus = "Failed: ESP32 was not connected!";
    }

    return res.json({ success: true, message: `File uploaded to Cloud! Transferring to ESP32...` });
});

app.listen(PORT, () => {
    console.log(`🚀 ESP32 OTA Server running on port ${PORT}`);
});
