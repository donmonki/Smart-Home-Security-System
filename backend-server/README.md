# 🏠 Backend Server (Home Assistant & MQTT)

This folder contains the core intelligence of our security project, orchestrated via Docker. It allows our team to share the exact same server environment across Mac, Linux, and Windows (via WSL).

---

## 📋 Prerequisites

### All Platforms

- **Docker Desktop** (or Docker Engine for Linux)
- **Docker Compose** (usually included with Docker Desktop)

### macOS

1. **Install Docker Desktop for Mac**
   - Download from [Docker's official website](https://www.docker.com/products/docker-desktop)
   - Install the `.dmg` file
   - Launch Docker Desktop and wait for it to start
   - Verify installation:
     ```bash
     docker --version
     docker-compose --version
     ```

### Linux

1. **Install Docker Engine**

   ```bash
   sudo apt-get update
   sudo apt-get install docker.io docker-compose
   sudo systemctl start docker
   sudo systemctl enable docker
   sudo usermod -aG docker $USER
   ```

   - Log out and back in for group changes to take effect

### Windows (WSL2 Setup) - **RECOMMENDED**

#### Step 1: Enable WSL2

1. **Open PowerShell as Administrator** and run:

   ```powershell
   wsl --install
   ```

   - This will install WSL2 and Ubuntu by default
   - Restart your computer when prompted

2. **Set WSL2 as the default version**:

   ```powershell
   wsl --set-default-version 2
   ```

3. **Create a Linux user account** when Ubuntu launches for the first time

#### Step 2: Install Docker Desktop for Windows

1. **Download Docker Desktop** from [Docker's official website](https://www.docker.com/products/docker-desktop)
2. **During installation**, ensure "Use WSL 2 instead of Hyper-V" is selected
3. **After installation**:
   - Open Docker Desktop
   - Go to **Settings → General**
   - Ensure "Use the WSL 2 based engine" is checked
   - Go to **Settings → Resources → WSL Integration**
   - Enable integration with your Ubuntu distribution
   - Click "Apply & Restart"

#### Step 3: Setup in WSL2

1. **Open Ubuntu from Windows Start Menu**
2. **Navigate to your project**:

   ```bash
   cd /mnt/c/Users/YourUsername/Documents/Smart-Home-Security-System/backend-server
   ```

   > **Tip**: Windows drives are mounted under `/mnt/` in WSL2

3. **Verify Docker is accessible**:
   ```bash
   docker --version
   docker-compose --version
   ```

#### WSL2 Prerequisites Checklist

- ✅ Windows 10 version 2004+ (Build 19041+) or Windows 11
- ✅ Virtualization enabled in BIOS
- ✅ WSL2 installed and set as default
- ✅ Docker Desktop installed with WSL2 integration enabled
- ✅ Ubuntu (or preferred Linux distro) running in WSL2

---

## 🚀 Quick Start (Running the Server)

### For macOS/Linux:

1. **Open Terminal** and navigate to this folder:

   ```bash
   cd backend-server
   ```

2. **Launch the stack**:
   ```bash
   docker-compose up -d
   ```

### For Windows (WSL2):

1. **Open Ubuntu** from Windows Start Menu

2. **Navigate to the project**:

   ```bash
   cd /mnt/c/Users/YourUsername/Documents/Smart-Home-Security-System/backend-server
   ```

3. **Launch the stack**:
   ```bash
   docker-compose up -d
   ```

### Verify Services Are Running

```bash
docker-compose ps
```

You should see both `homeassistant` and `mosquitto` containers running.

---

## 🌐 Accessing the Services

### Home Assistant

- **URL**: http://localhost:8123
- **First-time setup**: Create an admin account when you first visit
- **Configuration files**: `./home-assistant/`

### MQTT Broker (Mosquitto)

- **Host**: `localhost` (or your machine's IP for network access)
- **Port**: `1883`
- **Protocol**: MQTT
- **Configuration**: `./mosquitto/config/mosquitto.conf`

---

## 🛠️ Common Commands

### Start the services

```bash
docker-compose up -d
```

### Stop the services

```bash
docker-compose down
```

### View logs

```bash
# All services
docker-compose logs -f

# Specific service
docker-compose logs -f homeassistant
docker-compose logs -f mqtt
```

### Restart a specific service

```bash
docker-compose restart homeassistant
docker-compose restart mqtt
```

### Rebuild containers (after configuration changes)

```bash
docker-compose up -d --build
```

---

## 🔧 Troubleshooting

### Windows WSL2 Issues

**Problem**: "docker: command not found" in WSL

- **Solution**: Ensure Docker Desktop is running and WSL integration is enabled in Docker Desktop settings

**Problem**: Slow file access from `/mnt/c/`

- **Solution**: Consider cloning the repository directly in WSL's filesystem:
  ```bash
  cd ~
  git clone <repository-url>
  ```

**Problem**: Port already in use

- **Solution**: Check if another service is using port 8123 or 1883:

  ```bash
  # Windows PowerShell
  netstat -ano | findstr :8123

  # WSL/Linux/Mac
  lsof -i :8123
  ```

### General Docker Issues

**Problem**: Containers keep restarting

- **Solution**: Check logs for errors:
  ```bash
  docker-compose logs homeassistant
  ```

**Problem**: Changes in config files not reflected

- **Solution**: Restart the specific service:
  ```bash
  docker-compose restart homeassistant
  ```

**Problem**: Cannot access on network from other devices

- **Solution**: Use your computer's IP address instead of `localhost` and ensure firewall allows connections

---

## 📁 Folder Structure

```
backend-server/
├── docker-compose.yml          # Service orchestration
├── README.md                   # This file
├── home-assistant/            # Home Assistant configuration & data
│   ├── configuration.yaml
│   ├── automations.yaml
│   └── ...
└── mosquitto/                 # MQTT broker configuration & data
    ├── config/
    │   └── mosquitto.conf
    ├── data/
    └── log/
```

---

## 📚 Additional Resources

- [Home Assistant Documentation](https://www.home-assistant.io/docs/)
- [Mosquitto MQTT Broker](https://mosquitto.org/)
- [Docker Compose Documentation](https://docs.docker.com/compose/)
- [WSL2 Documentation](https://docs.microsoft.com/en-us/windows/wsl/)

---

## 👥 Team Notes

- All configuration changes should be committed to the repository
- Use branches for development and create pull requests before merging to main
- Test your changes locally before pushing
- Document any new integrations or automations in the appropriate configuration files
