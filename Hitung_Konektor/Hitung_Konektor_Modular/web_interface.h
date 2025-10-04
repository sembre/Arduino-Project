// web_interface.h - Interface web yang ringan untuk Camera Stream dan File Manager
#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <title>ESP32-S3 Camera & File Manager</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background: #f5f5f5;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 10px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            text-align: center;
        }
        .content {
            display: flex;
            min-height: 600px;
        }
        .camera-panel {
            flex: 1;
            padding: 20px;
            border-right: 1px solid #ddd;
        }
        .file-panel {
            flex: 1;
            padding: 20px;
            background: #fafafa;
        }
        .camera-stream {
            width: 100%;
            max-width: 640px;
            border: 2px solid #ddd;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }
        .file-manager {
            border: 1px solid #ddd;
            border-radius: 8px;
            background: white;
            height: 500px;
            overflow: hidden;
        }
        .file-header {
            background: #f8f9fa;
            padding: 15px;
            border-bottom: 1px solid #ddd;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .file-list {
            height: 400px;
            overflow-y: auto;
            padding: 10px;
        }
        .file-item {
            display: flex;
            align-items: center;
            padding: 8px 12px;
            margin: 2px 0;
            border-radius: 5px;
            cursor: pointer;
            transition: background 0.2s;
        }
        .file-item:hover {
            background: #e3f2fd;
        }
        .file-icon {
            margin-right: 10px;
            font-size: 16px;
        }
        .file-name {
            flex: 1;
            font-size: 14px;
        }
        .file-size {
            font-size: 12px;
            color: #666;
            margin-left: 10px;
        }
        .btn {
            background: #007bff;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 14px;
            margin: 2px;
        }
        .btn:hover {
            background: #0056b3;
        }
        .btn-sm {
            padding: 4px 8px;
            font-size: 12px;
        }
        .path-nav {
            background: #e9ecef;
            padding: 10px;
            font-size: 14px;
            border-radius: 5px;
            margin-bottom: 10px;
        }
        .status {
            position: fixed;
            top: 20px;
            right: 20px;
            background: white;
            padding: 10px;
            border-radius: 5px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.2);
            z-index: 1000;
        }
        .upload-progress {
            background: #e3f2fd;
            border: 1px solid #2196F3;
            border-radius: 5px;
            padding: 8px 12px;
            margin: 2px 0;
            font-size: 12px;
        }
        .btn-danger {
            background: #dc3545;
        }
        .btn-danger:hover {
            background: #c82333;
        }
        @media (max-width: 768px) {
            .content {
                flex-direction: column;
            }
            .camera-panel {
                border-right: none;
                border-bottom: 1px solid #ddd;
            }
        }
    </style>
</head>
<body>
    <div class="status">
        <span id="connectionStatus">🟡 Connecting...</span>
    </div>

    <div class="container">
        <div class="header">
            <h1>ESP32-S3 Camera & File Manager</h1>
            <p>Live Camera Stream & SD Card File Manager</p>
        </div>
        
        <div class="content">
            <!-- Camera Panel -->
            <div class="camera-panel">
                <h2>📹 Live Camera Stream</h2>
                <img id="cameraStream" class="camera-stream" src="/stream" alt="Camera Stream">
                <div style="margin-top: 15px;">
                    <button class="btn" onclick="refreshStream()">🔄 Refresh Stream</button>
                    <button class="btn" onclick="captureImage()">📸 Capture Image</button>
                    <button class="btn" onclick="testCamera()">🧪 Test Camera</button>
                </div>
                <div id="captureStatus" style="margin-top: 10px;"></div>
                <div id="cameraTestStatus" style="margin-top: 5px;"></div>
            </div>
            
            <!-- File Manager Panel -->
            <div class="file-panel">
                <h2>📁 SD Card File Manager</h2>
                <div class="file-manager">
                    <div class="file-header">
                        <button class="btn btn-sm" onclick="goHome()">🏠 Root</button>
                        <button class="btn btn-sm" onclick="goUp()">⬆️ Up</button>
                        <button class="btn btn-sm" onclick="refreshFiles()">🔄 Refresh</button>
                        <button class="btn btn-sm" onclick="reconnectSDCard()">🔗 Reconnect SD</button>
                        <button class="btn btn-sm" onclick="showCreateFolder()">📁 New Folder</button>
                        <input type="file" id="fileInput" style="display: none;" onchange="uploadFile()" multiple>
                        <button class="btn btn-sm" onclick="document.getElementById('fileInput').click()">📤 Upload</button>
                    </div>
                    <div class="path-nav">
                        <strong>Current Path:</strong> <span id="currentPath">/</span>
                    </div>
                    <div class="file-list" id="fileList">
                        <div style="text-align: center; color: #666; margin-top: 50px;">
                            <div>📂 Loading files...</div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        let currentPath = '/';
        
        // Initialize
        document.addEventListener('DOMContentLoaded', function() {
            refreshFiles();
            startConnectionMonitor();
        });
        
        // Connection monitoring
        function startConnectionMonitor() {
            setInterval(async () => {
                try {
                    const response = await fetch('/system_info');
                    const status = response.ok ? '🟢 Connected' : '🔴 Disconnected';
                    document.getElementById('connectionStatus').textContent = status;
                } catch {
                    document.getElementById('connectionStatus').textContent = '🔴 Disconnected';
                }
            }, 5000);
        }
        
        // Camera functions
        function refreshStream() {
            const img = document.getElementById('cameraStream');
            const timestamp = new Date().getTime();
            img.onerror = function() {
                console.log('Camera stream error, retrying...');
                setTimeout(() => {
                    img.src = '/stream?' + new Date().getTime();
                }, 2000);
            };
            img.src = '/stream?' + timestamp;
        }
        
        function captureImage() {
            document.getElementById('captureStatus').innerHTML = '<div style="color: blue;">📸 Capturing...</div>';
            
            fetch('/capture', { 
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                cache: 'no-cache'
            })
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP error! status: ${response.status}`);
                    }
                    return response.json();
                })
                .then(data => {
                    if (data.success) {
                        document.getElementById('captureStatus').innerHTML = 
                            '<div style="color: green;">✅ Image saved: ' + data.filename + '</div>';
                        refreshFiles(); // Refresh file list
                    } else {
                        document.getElementById('captureStatus').innerHTML = 
                            '<div style="color: red;">❌ Error: ' + data.error + '</div>';
                    }
                })
                .catch(error => {
                    console.error('Capture error:', error);
                    document.getElementById('captureStatus').innerHTML = 
                        '<div style="color: red;">❌ Capture failed: ' + error.message + '</div>';
                });
        }
        
        function testCamera() {
            document.getElementById('cameraTestStatus').innerHTML = '<div style="color: blue;">🧪 Testing camera...</div>';
            
            fetch('/camera_test')
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP error! status: ${response.status}`);
                    }
                    return response.json();
                })
                .then(data => {
                    if (data.success) {
                        document.getElementById('cameraTestStatus').innerHTML = 
                            '<div style="color: green;">✅ Camera OK: ' + data.width + 'x' + data.height + ' (' + data.size + ' bytes)</div>';
                    } else {
                        document.getElementById('cameraTestStatus').innerHTML = 
                            '<div style="color: red;">❌ Camera test failed: ' + data.error + '</div>';
                    }
                })
                .catch(error => {
                    console.error('Camera test error:', error);
                    document.getElementById('cameraTestStatus').innerHTML = 
                        '<div style="color: red;">❌ Camera test failed: ' + error.message + '</div>';
                });
        }
        
        // File manager functions
        function refreshFiles() {
            document.getElementById('fileList').innerHTML = 
                '<div style="text-align: center; color: #666; margin-top: 50px;"><div>📂 Loading files...</div></div>';
            
            fetch('/files?path=' + encodeURIComponent(currentPath))
                .then(response => {
                    if (!response.ok) {
                        throw new Error('HTTP ' + response.status + ': ' + response.statusText);
                    }
                    return response.json();
                })
                .then(data => {
                    if (data.success) {
                        displayFiles(data.files);
                        document.getElementById('currentPath').textContent = currentPath;
                        console.log('Files loaded successfully:', data.count || data.files.length, 'files');
                    } else {
                        document.getElementById('fileList').innerHTML = 
                            '<div style="text-align: center; color: red; margin-top: 50px;"><div>❌ Error: ' + (data.error || 'Unknown error') + '</div><button class="btn" onclick="refreshFiles()" style="margin-top: 10px;">🔄 Retry</button></div>';
                    }
                })
                .catch(error => {
                    console.error('File list error:', error);
                    document.getElementById('fileList').innerHTML = 
                        '<div style="text-align: center; color: red; margin-top: 50px;"><div>❌ Connection error: ' + error.message + '</div><button class="btn" onclick="refreshFiles()" style="margin-top: 10px;">🔄 Retry</button></div>';
                });
        }
        
        function displayFiles(files) {
            const fileList = document.getElementById('fileList');
            
            if (files.length === 0) {
                fileList.innerHTML = '<div style="text-align: center; color: #666; margin-top: 50px;"><div>📂 Empty folder</div></div>';
                return;
            }
            
            let html = '';
            files.forEach(file => {
                const icon = file.isDir ? '📁' : '📄';
                const sizeText = file.isDir ? '' : formatFileSize(file.size);
                
                html += '<div class="file-item">';
                html += '<span class="file-icon">' + icon + '</span>';
                html += '<span class="file-name" onclick="' + (file.isDir ? 'openFolder' : 'downloadFile') + '(\'' + file.name + '\')" style="flex: 1; cursor: pointer;">' + file.name + '</span>';
                if (sizeText) {
                    html += '<span class="file-size">' + sizeText + '</span>';
                }
                if (!file.isDir) {
                    html += '<button class="btn btn-sm" onclick="downloadFile(\'' + file.name + '\')" style="margin-left: 5px;">💾</button>';
                    html += '<button class="btn btn-sm" onclick="deleteFile(\'' + file.name + '\')" style="margin-left: 5px; background: #dc3545;">🗑️</button>';
                }
                html += '</div>';
            });
            
            fileList.innerHTML = html;
        }
        
        function openFolder(folderName) {
            if (currentPath === '/') {
                currentPath = '/' + folderName;
            } else {
                currentPath = currentPath + '/' + folderName;
            }
            refreshFiles();
        }
        
        function downloadFile(fileName) {
            const filePath = currentPath === '/' ? '/' + fileName : currentPath + '/' + fileName;
            window.open('/download?file=' + encodeURIComponent(filePath), '_blank');
        }
        
        function goHome() {
            currentPath = '/';
            refreshFiles();
        }
        
        function goUp() {
            if (currentPath !== '/') {
                const parts = currentPath.split('/');
                parts.pop();
                currentPath = parts.join('/') || '/';
                refreshFiles();
            }
        }
        
        function showCreateFolder() {
            const folderName = prompt('Enter folder name:');
            if (folderName && folderName.trim()) {
                createFolder(folderName.trim());
            }
        }
        
        function createFolder(folderName) {
            const formData = new FormData();
            formData.append('path', currentPath);
            formData.append('name', folderName);
            
            fetch('/create_folder', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    refreshFiles();
                } else {
                    alert('Error creating folder: ' + data.error);
                }
            })
            .catch(error => {
                alert('Error: ' + error.message);
            });
        }
        
        function formatFileSize(bytes) {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
        }
        
        function uploadFile() {
            const fileInput = document.getElementById('fileInput');
            const files = fileInput.files;
            
            if (files.length === 0) {
                return;
            }
            
            Array.from(files).forEach((file, index) => {
                const formData = new FormData();
                formData.append('file', file);
                formData.append('path', currentPath);
                
                // Create progress indicator
                const uploadStatus = document.createElement('div');
                uploadStatus.className = 'upload-progress';
                uploadStatus.innerHTML = '📤 Uploading: ' + file.name + ' (' + formatFileSize(file.size) + ')';
                document.getElementById('fileList').insertBefore(uploadStatus, document.getElementById('fileList').firstChild);
                
                fetch('/upload', {
                    method: 'POST',
                    body: formData,
                    // Add timeout for large files
                    signal: AbortSignal.timeout(30000) // 30 second timeout
                })
                .then(response => {
                    if (!response.ok) {
                        throw new Error('HTTP ' + response.status + ': ' + response.statusText);
                    }
                    return response.json();
                })
                .then(data => {
                    uploadStatus.remove();
                    if (data.success) {
                        console.log('Upload successful:', file.name);
                        // Refresh files after last upload
                        if (index === files.length - 1) {
                            setTimeout(refreshFiles, 500);
                        }
                    } else {
                        alert('Upload failed for ' + file.name + ': ' + (data.error || 'Unknown error'));
                    }
                })
                .catch(error => {
                    uploadStatus.remove();
                    console.error('Upload error:', error);
                    alert('Upload error for ' + file.name + ': ' + error.message);
                });
            });
            
            // Clear file input
            fileInput.value = '';
        }
        
        function deleteFile(fileName) {
            if (!confirm('Are you sure you want to delete "' + fileName + '"?\\n\\nThis action cannot be undone.')) {
                return;
            }
            
            const filePath = currentPath === '/' ? '/' + fileName : currentPath + '/' + fileName;
            
            fetch('/delete?file=' + encodeURIComponent(filePath), {
                method: 'DELETE'
            })
            .then(response => {
                if (!response.ok) {
                    throw new Error('HTTP ' + response.status + ': ' + response.statusText);
                }
                return response.json();
            })
            .then(data => {
                if (data.success) {
                    console.log('File deleted successfully:', fileName);
                    refreshFiles();
                } else {
                    alert('Delete failed for "' + fileName + '": ' + (data.error || 'Unknown error'));
                }
            })
            .catch(error => {
                console.error('Delete error:', error);
                alert('Delete error for "' + fileName + '": ' + error.message);
            });
        }
        
        function reconnectSDCard() {
            if (!confirm('Reconnect SD card? This may take a few seconds...')) {
                return;
            }
            
            // Show loading state
            const fileList = document.getElementById('fileList');
            const originalContent = fileList.innerHTML;
            fileList.innerHTML = '<div style="text-align: center; color: blue; margin-top: 50px;"><div>🔗 Reconnecting SD card...</div></div>';
            
            fetch('/sd_reconnect', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                }
            })
            .then(response => {
                if (!response.ok) {
                    throw new Error('HTTP ' + response.status + ': ' + response.statusText);
                }
                return response.json();
            })
            .then(data => {
                if (data.success) {
                    alert('SD card reconnected successfully! Mode: ' + (data.mode || 'Unknown'));
                    refreshFiles();
                } else {
                    alert('Failed to reconnect SD card: ' + (data.error || 'Unknown error'));
                    fileList.innerHTML = originalContent;
                }
            })
            .catch(error => {
                console.error('SD reconnect error:', error);
                alert('SD reconnect error: ' + error.message);
                fileList.innerHTML = originalContent;
            });
        }
    </script>
</body>
</html>
)rawliteral";

#endif
