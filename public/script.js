const dropZone = document.getElementById('drop-zone');
const fileInput = document.getElementById('file-input');
const progressContainer = document.getElementById('progress-container');
const progressFill = document.getElementById('progress-fill');
const progressText = document.getElementById('progress-text');
const logConsole = document.getElementById('log-console');
const statusBox = document.getElementById('status-box');
const statusText = document.getElementById('status-text');

let isUploading = false;

setInterval(checkStatus, 2000);

async function checkStatus() {
    try {
        const res = await fetch('/status');
        const data = await res.json();
        
        if (data.connected_devices > 0) {
            statusBox.classList.add('online');
            statusText.innerText = "ESP32 Online";
        } else {
            statusBox.classList.remove('online');
            statusText.innerText = "Waiting for ESP32";
        }

        // Update the log with ESP32's actual progress
        const currentLog = logConsole.lastChild ? logConsole.lastChild.innerText : "";
        if (data.download_status !== "Idle" && currentLog !== `> ${data.download_status}`) {
            log(data.download_status);
        }

    } catch (e) {
        statusBox.classList.remove('online');
        statusText.innerText = "Server Offline";
    }
}

function log(msg, isError = false) {
    const p = document.createElement('p');
    p.className = isError ? 'log-entry error' : 'log-entry';
    p.innerText = `> ${msg}`;
    logConsole.appendChild(p);
    logConsole.scrollTop = logConsole.scrollHeight;
}

dropZone.addEventListener('click', () => fileInput.click());
dropZone.addEventListener('dragover', (e) => { e.preventDefault(); dropZone.classList.add('dragover'); });
dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.remove('dragover');
    if (e.dataTransfer.files.length) uploadFile(e.dataTransfer.files[0]);
});
fileInput.addEventListener('change', () => {
    if (fileInput.files.length) uploadFile(fileInput.files[0]);
});

function uploadFile(file) {
    if (isUploading) return;
    isUploading = true;
    
    log(`Uploading ${file.name} to Cloud...`);
    
    const formData = new FormData();
    formData.append('ota_file', file);
    
    progressContainer.style.display = 'block';
    
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/upload', true);
    
    xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
            const percentComplete = (e.loaded / e.total) * 100;
            progressFill.style.width = percentComplete + '%';
            progressText.innerText = Math.round(percentComplete) + '% to Cloud';
        }
    };
    
    xhr.onload = function() {
        if (xhr.status === 200) {
            progressFill.style.background = 'var(--success)';
            progressText.innerText = 'Cloud Upload Done!';
            setTimeout(() => {
                progressContainer.style.display = 'none';
                progressFill.style.width = '0%';
                progressFill.style.background = 'linear-gradient(90deg, var(--primary), var(--accent))';
            }, 3000);
        } else {
            log('Upload failed!', true);
        }
        isUploading = false;
        fileInput.value = '';
    };
    xhr.onerror = function() { log('Network error!', true); isUploading = false; };
    xhr.send(formData);
}

checkStatus();
