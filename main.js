const { input, password, select } = require('@inquirer/prompts');
const fs = require('fs-extra');
const { SerialPort } = require('serialport');
const path = require('path');
const { spawn } = require('child_process');
const { exec, execSync } = require('child_process');
const dotenv = require('dotenv');

const envFilePath = path.resolve(__dirname, '.env');
const envTemplate = `WIFI_SSID=
WIFI_PASSWORD=
BULB_COLOR_IP=10.0.0.182
BULB_WHITE_IP=10.0.0.229
PORT=
`;

const projectDir = path.resolve(__dirname, 'src/firmware');
const headerFilePath = path.join(projectDir, 'credentials.h');

const createHeaderFile = () => {
  const headerContent = `
#ifndef CREDENTIALS_H
#define CREDENTIALS_H

const char* WIFI_SSID = "${process.env.WIFI_SSID}";
const char* WIFI_PASSWORD = "${process.env.WIFI_PASSWORD}";
const char* BULB_COLOR_IP = "${process.env.BULB_COLOR_IP}";
const char* BULB_WHITE_IP = "${process.env.BULB_WHITE_IP}";

#endif // CREDENTIALS_H
  `;
  fs.writeFileSync(headerFilePath, headerContent, 'utf8');
  console.log('✅ Credentials updated');
};

const checkArduinoCLIInstalled = () => {
  return new Promise((resolve, reject) => {
    const proc = spawn('arduino-cli', ['version'], { shell: true });
    proc.on('error', (err) => {
      console.error('🛑 Arduino CLI is not installed. Please install it from https://www.arduino.cc/en/software');
      reject(err);
    });
    proc.on('exit', (code) => {
      if (code === 0) {
        resolve();
      } else {
        console.error('🛑 Arduino CLI is not installed. Please install it from https://www.arduino.cc/en/software');
        reject(new Error('Arduino CLI is not installed.'));
      }
    });
  });
};

const checkEnvFile = () => {
  if (!fs.existsSync(envFilePath)) {
    console.log('📁 .env file not found, creating...');
    fs.writeFileSync(envFilePath, envTemplate);
  }
  dotenv.config({ path: envFilePath });
};

const promptForEnvVariables = async () => {
  const envVars = [
    { name: 'WIFI_SSID', message: 'Enter WiFi SSID:', initial: process.env.WIFI_SSID || '' },
    { name: 'WIFI_PASSWORD', message: 'Enter WiFi Password:', initial: process.env.WIFI_PASSWORD || '', type: 'password' },
    { name: 'BULB_COLOR_IP', message: 'Enter Bulb Color IP:', initial: process.env.BULB_COLOR_IP || '10.0.0.182' },
    { name: 'BULB_WHITE_IP', message: 'Enter Bulb White IP:', initial: process.env.BULB_WHITE_IP || '10.0.0.229' }
  ];

  for (const envVar of envVars) {
    if (!process.env[envVar.name]) {
      const answer = await (envVar.type === 'password' ? password : input)({
        message: envVar.message,
        initial: envVar.initial,
        validate: (value) => value ? true : 'This field is required',
        onKeypress: (key, rl) => {
          if (key.sequence === '\u0003') {
            rl.close();
            process.exit();
          }
        }
      });
      process.env[envVar.name] = answer;
    }
  }

  const envContent = `WIFI_SSID=${process.env.WIFI_SSID}
WIFI_PASSWORD=${process.env.WIFI_PASSWORD}
BULB_COLOR_IP=${process.env.BULB_COLOR_IP}
BULB_WHITE_IP=${process.env.BULB_WHITE_IP}
PORT=${process.env.PORT || ''}
`;

  fs.writeFileSync(envFilePath, envContent);
  dotenv.config({ path: envFilePath });
};

const spawnProcess = (command, args, options = {}, silent = false) => {
  return new Promise((resolve, reject) => {
    const proc = spawn(command, args, { ...options, shell: true });
    proc.stdout.on('data', (data) => {
      if (!silent)
        console.log(`${data.toString()}`);
    });
    proc.stderr.on('data', (data) => {
      console.error(`${data.toString()}`);
    });
    proc.on('close', (code) => {
      if (code !== 0) {
        reject(new Error(`Process exited with code ${code}`));
      } else {
        resolve(`Process completed successfully with code ${code}`);
      }
    });
  });
};

const buildProject = async () => {
  createHeaderFile();
  console.log('🔨 Building project...');
  try {
    await spawnProcess('arduino-cli', ['compile', '--fqbn', 'esp8266:esp8266:nodemcu', projectDir]);
    console.log('✅ Build completed successfully.');
  } catch (error) {
    console.error(`❌ Build error: ${error}`);
  }
};

const uploadProject = async () => {
  await ensurePortIsAlive();
  createHeaderFile();
  console.log('📤 Building and uploading project...');
  try {
    await spawnProcess('arduino-cli', ['compile', '--fqbn', 'esp8266:esp8266:nodemcu', projectDir]);
    await spawnProcess('arduino-cli', ['upload', '--fqbn', 'esp8266:esp8266:nodemcu', '--port', process.env.PORT, projectDir]);
    console.log('✅ Upload completed successfully.');
  } catch (error) {
    console.error(`❌ Upload error: ${error}`);
    throw error;
  }
};

const getAvailablePorts = async () => {
  return new Promise((resolve, reject) => {
    exec('ls /dev/tty.*', (error, stdout, stderr) => {
      if (error) {
        reject(stderr);
      } else {
        resolve(stdout.split('\n').filter(Boolean));
      }
    });
  });
};

const updateEnvFile = (key, value) => {
  const envConfig = dotenv.parse(fs.readFileSync(envFilePath));
  envConfig[key] = value;
  const newEnvContent = Object.keys(envConfig)
    .map(key => `${key}=${envConfig[key]}`)
    .join('\n');
  fs.writeFileSync(envFilePath, newEnvContent);
};

const promptForPort = async () => {
  const ports = await getAvailablePorts();
  const portChoice = await select({
    message: '🔌 Select a COM port:',
    choices: ports.map(port => ({ title: port, value: port })),
  });

  return portChoice;
};

async function isPortAlive(portName) {
  try {
    const ports = await fs.readdir('/dev');
    const availablePorts = ports.filter(port => port.startsWith('tty.'));
    const fullPortPaths = availablePorts.map(port => path.join('/dev', port));

    if (fullPortPaths.includes(portName)) {
      return true;
    } else {
      console.log(`❌ Port ${portName} does not exist.`);
      return false;
    }
  } catch (err) {
    console.error(`❌ Error accessing ports: ${err.message}`);
    return false;
  }
}

const ensurePortIsAlive = async () => {
  if (!process.env.PORT || !(await isPortAlive(process.env.PORT))) {
    process.env.PORT = await promptForPort();

    updateEnvFile('PORT', process.env.PORT);
    await ensurePortIsAlive(); // Recursively ensure the selected port is alive
  }
};

async function monitorProject() {
  await ensurePortIsAlive();
  const portName = process.env.PORT;
  return new Promise((resolve, reject) => {
    const port = new SerialPort({ path: portName, baudRate: 115200, autoOpen: false });

    port.open((err) => {
      if (err) {
        console.error(`❌ Error opening port: ${err.message}`);
        return reject(err);
      }

      const monitor = spawn('cat', [portName], { shell: true });

      monitor.stdout.on('data', (data) => {
        console.log(`${data}`);
      });

      monitor.stderr.on('data', (data) => {
        console.error(`❌ Error: ${data}`);
      });

      monitor.on('close', (code) => {
        console.log(`📴 Monitoring stopped with code ${code}`);
        resolve();
      });

      port.on('error', (err) => {
        console.error(`❌ Port error: ${err.message}`);
        reject(err);
      });
    });
  });
}

const checkLibraryVersion = async (libraryName, requiredVersion) => {
  try {
    const result = await spawnProcess('arduino-cli', ['lib', 'list'], {}, true);
    const installedLibraries = result.stdout;
    const regex = new RegExp(`^${libraryName}\\s+([\\d.]+)`, 'm');
    const match = regex.exec(installedLibraries);
    if (match && match[1] === requiredVersion) {
      return true;
    } else {

      await spawnProcess('arduino-cli', ['lib', 'install', `${libraryName}@${requiredVersion}`], {}, true);
      return true;
    }
  } catch (error) {
    console.error(`❌ Error checking/installing library ${libraryName} version ${requiredVersion}: ${error.message}`);
    return false;
  }
};


const main = async () => {
  try {
    await checkArduinoCLIInstalled();

    const requiredLibrary = 'HomeKit-ESP8266';
    const requiredVersion = '1.1.0';
    const isLibraryInstalled = await checkLibraryVersion(requiredLibrary, requiredVersion);

    if (!isLibraryInstalled) {
      console.error(`Failed to ensure required library ${requiredLibrary} version ${requiredVersion} is installed.`);
      process.exit(1);
    }

    checkEnvFile();
    await promptForEnvVariables();

    const command = process.argv[2];

    switch (command) {
      case 'build':
        await buildProject();
        break;
      case 'upload':
        await uploadProject();
        break;
      case 'monitor':
        await monitorProject();
        break;
      case 'start':
        await uploadProject();
        await monitorProject();
        break;
      default:
        console.log('❓ Unknown command. Use "build", "upload", "monitor", or "start".');
    }
  } catch (error) {
    console.error(error.message);
  }
};

main();
