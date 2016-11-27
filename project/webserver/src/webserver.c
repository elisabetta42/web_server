/* webserver */
/* Elisabetta Seggioli */
/* Jan-Philipp Heilmann */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>

#define BUFFERSIZE 1024
/* define quene lenght? */
typedef enum { false, true } bool;
typedef struct configuration {
	char * logfile;
	int port;
	bool daemon;
	char * requestHandlingMethod;
	char * documentRootDirectory;

} configuration;
typedef struct threadArgs {
	int clientSocketDescriptor;
	configuration cfg;
} threadArgs;
typedef struct LogObj {
	char * IP;
	char * userID;
	char * date;
	char *request;
	int result_code;
	int bytes;
} LogObj;
configuration readConfigurationFile(configuration cfg);
void * threadHelp(void * thrdArgs);

main(int argc, char * argv[])
{
	fprintf(stdout, "Webserver is starting.\n");
	int serverSocketDescriptor = 0;
	struct sockaddr_in serverAddress;
	configuration cfg;
	cfg.logfile = "-";
	cfg.port = 80;
	cfg.requestHandlingMethod = "thread";
	cfg.documentRootDirectory = "./";
	cfg.daemon = false;
	
	cfg = readConfigurationFile(cfg);

	int i;
	for (i = 1; i < argc; i++) { /* Skip argv[0] (program name). */
		if (strcmp(argv[i], "-h") == 0) { /* Print help text. */
			printHelp();
		} else if (strcmp(argv[i], "-p") == 0) { /* Set port. */
			if (i + 1 <= argc - 1) {
				i++;
				cfg.port = atoi(argv[i]); /* Port set to zero, if no valid conversion could be performed. */
			} else {
				fprintf(stderr, "No port specified, using default.\n");
			}
		} else if (strcmp(argv[i], "-d") == 0) { /* Run as a daemon.*/
			cfg.daemon = true;
		} else if (strcmp(argv[i], "-l") == 0) { /* Set logfile. */
			if (i + 1 <= argc - 1) {
				i++;
				int valueSize = strlen(argv[i]);
				cfg.logfile = malloc(valueSize);
				strcpy(cfg.logfile, argv[i]);
			} else {
				fprintf(stderr, "No logfile specified, logging will be output to syslog.\n");
			}
		} else if (strcmp(argv[i], "-s") == 0) { /* Set request handling method.*/
			if (i + 1 <= argc - 1) {
				i++;
				int valueSize = strlen(argv[i]);
				cfg.requestHandlingMethod = malloc(valueSize);
				strcpy(cfg.requestHandlingMethod, argv[i]);
			} else {
				fprintf(stderr, "No request handling method specified, using default.\n");
			}
		}
		else {
			fprintf(stderr, "Unknow argument: %s\n", argv[i]);
			exit(-1);
		}
	}

	/* Display settings */
	fprintf(stdout, "Logfile: %s\n", cfg.logfile);
	fprintf(stdout, "Run as a daemon: %s\n", cfg.daemon ? "true" : "false");
	fprintf(stdout, "Port: %d\n", cfg.port);
	fprintf(stdout, "Request handling method: %s\n", cfg.requestHandlingMethod);
	fprintf(stdout, "Document root directory: %s\n", cfg.documentRootDirectory);

	/*create socket */
	if((serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Failed to create socket.\n");
		exit(-1);
	}
	memset(&serverAddress, '0', sizeof(serverAddress) + 1); /*overwrites serverAddress with zeros?*/

	/* set to IPv4 */
	serverAddress.sin_family = AF_INET;
	/* listen to all local interfaces */
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	/* set port number */
	serverAddress.sin_port = htons(cfg.port);

	/* assign the details specified in serverAddress to the socket */
	if(bind(serverSocketDescriptor, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
		fprintf(stderr, "Failed to bind address to socket.\n");
		exit(-1);
	}
	/* set socket to listing with a queue for upto 2147483647 (2^31 − 1) client connections */
	if(listen(serverSocketDescriptor, 2147483647) == -1) {
		fprintf(stderr, "Failed to start listing.\n");
		exit(-1);
	}
	fprintf(stdout, "Webserver started.\n");
	if (cfg.daemon) {
		runAsDaemon();
	}
	if (strcmp(cfg.requestHandlingMethod, "fork") == 0) {
		requestHandlingUsingFork(serverSocketDescriptor, cfg);
	} else if (strcmp(cfg.requestHandlingMethod, "thread") == 0) {
		requestHandlingUsingThread(serverSocketDescriptor, cfg);
	} else if (strcmp(cfg.requestHandlingMethod, "prefork") == 0) {
		requestHandlingUsingPrefork(serverSocketDescriptor, cfg);
	} else if (strcmp(cfg.requestHandlingMethod, "mux") == 0) {
		requestHandlingUsingMux(serverSocketDescriptor, cfg);
	} else {
		fprintf(stderr, "Unknown request handling method specified.\n");
	}
	// free auf alle pointer
}

configuration readConfigurationFile(configuration cfg) {
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	char * configFile = "lab3-config";// WRONG file name (leading dot missing)
	fp = fopen(configFile, "r");
	if (fp == NULL) {
		fprintf(stderr, "Could not find configuration file: %s\n", configFile);
		return cfg;
	}
	char * delimiter = ":";
	while ((read = getline(&line, &len, fp)) != -1) {
		char *value;
		char *key = strtok_r(line, delimiter, &value);
		strtok(value, "\n"); /* Remove new line(\n). */
		int valueSize = strlen(value);
		if (strcmp(key, "documentRootDirectory") == 0) {
			cfg.documentRootDirectory = malloc(valueSize);
			strcpy(cfg.documentRootDirectory, value);
		} else if (strcmp(key, "port") == 0) {
			cfg.port = atoi(value);
		} else if (strcmp(key, "requestHandlingMethod") == 0) {
			cfg.requestHandlingMethod = malloc(valueSize);
			strcpy(cfg.requestHandlingMethod, value);
		}
		// free line here?
	}
	fclose(fp);
	if (line) {
		free(line);
	}
	return cfg;
}

printHelp() {
	fprintf(stdout, "Webserver commands list. All commands are optional.\n");
	fprintf(stdout, "-h Print help text.\n");
	fprintf(stdout, "-p port Listen to port number port.\n");
	fprintf(stdout, "-d Run as a daemon instead of as a normal program.\n");
	fprintf(stdout, "-l logfile Log to logfile. If this option is not specified, logging will be output to syslog, which is the default.\n");
	fprintf(stdout, "-s [fork | thread | prefork | mux] Select request handling method.\n");
	fprintf(stdout, "\n");
}
log_into_file(log_object *c){

}

runAsDaemon(const char *cmd) {
	pid_t processId = 0;
	struct rlimit maxFileDescriptors;
	struct sigaction signalAction;

	umask(0);
	if (getrlimit(RLIMIT_NOFILE, &maxFileDescriptors) < 0) {
        fprintf(stderr, "Failed to get maximum number of file descriptors.\n");
        exit(-1);
    }
	if ((processId = fork()) < 0) {
		fprintf(stderr, "Failed to create child process.\n");
		exit(-1);
	} else if (processId > 0) {
		fprintf(stdout, "Process ID: %d\n", processId);
		exit(0);
	}
	if(setsid() < 0) {
		exit(-1);
	}
	signalAction.sa_handler = SIG_IGN;
    sigemptyset(&signalAction.sa_mask);
    signalAction.sa_flags = 0;
    if (sigaction(SIGHUP, &signalAction, NULL) < 0) {
        fprintf(stderr, "Failed to ignore SIGHUP\n");
        exit(-1);
    }
    if ((processId = fork()) < 0){
        perror("Can't fork\n");
        exit(1);
    } else if (processId != 0) {/* parent */
        exit(0);
    }
	if (chdir("/") < 0){
        fprintf(stderr, "Failed to change current working directory to root.\n");
        exit(-1);
    }
    if (maxFileDescriptors.rlim_max == RLIM_INFINITY)
        maxFileDescriptors.rlim_max = 1024;
    int i;
    for (i = 0; i < maxFileDescriptors.rlim_max; i++) {
        close(i);
    }

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}

requestHandlingUsingFork(int serverSocketDescriptor, configuration cfg) {
	int clientSocketDescriptor = 0;
	while(1) {
		if ((clientSocketDescriptor = accept(serverSocketDescriptor, (struct sockaddr*)NULL, NULL)) == -1) {
			fprintf(stderr, "Failed to establish a connection.\n");
		}
		pid_t processId = fork();
		if (processId == 0){ 	/* child process */
			handleActualRequest(clientSocketDescriptor, cfg);
		} else if (processId > 0) { 			/* parent process */
			close(clientSocketDescriptor);
		} else {
			fprintf(stderr, "Failed to fork().\n");
		}
	}
}

requestHandlingUsingThread(int serverSocketDescriptor, configuration cfg) {
	int clientSocketDescriptor = 0;
	while(1) {
		if ((clientSocketDescriptor = accept(serverSocketDescriptor, (struct sockaddr*)NULL, NULL)) == -1) {
			fprintf(stderr, "Failed to establish a connection.\n");
		}
		pthread_t handleRequestThread;
		threadArgs thrdArgs;
		thrdArgs.clientSocketDescriptor = clientSocketDescriptor;
		thrdArgs.cfg = cfg;
		int rc = pthread_create(&handleRequestThread, NULL, &threadHelp, (void*)&thrdArgs);
		if(rc) {
			close(clientSocketDescriptor);
			fprintf(stderr, "Failed to create pthread.\n");
		}
	}
}

void * threadHelp(void * thrdArgs) {
	threadArgs * thrdArgsPointer = (threadArgs*)thrdArgs;
	handleActualRequest(thrdArgsPointer->clientSocketDescriptor, thrdArgsPointer->cfg);
	pthread_exit(NULL);
}

requestHandlingUsingPrefork() {
	fprintf(stderr, "Prefork not supported.\n");
	printHelp();
	exit(3);
}

requestHandlingUsingMux() {
	fprintf(stderr, "Mux not supported.\n");
	printHelp();
	exit(3);
}

handleActualRequest(int clientSocketDescriptor, configuration cfg) {
	/* from this point no outputs to stdout, all to logfile */
	char receiveBuffer[BUFFERSIZE];
	memset(receiveBuffer, '0', sizeof(receiveBuffer) + 1); /*overwrites receiveBuffer with zeros?*///needed?
	printf("Connection happened\n");// in logfile
	if (recv(clientSocketDescriptor, receiveBuffer, sizeof(receiveBuffer), 0) == -1) {
		fprintf(stderr, "Failed to receive.\n");
	}
	char * requestType;
	char * requestedFile;

	char * delimiter = " ";
	requestType = strtok(receiveBuffer, delimiter);
	requestedFile = strtok(NULL, delimiter);
	if (strcmp(requestedFile, "/") == 0) {
		requestedFile = "/index.html";
	}
	char * protocolWithVersion;
	protocolWithVersion = strtok(NULL, delimiter);

	char * statusCodes;
	char * fileWithPath;
	if (strncmp(protocolWithVersion, "HTTP/", 5) == 0) {
		fileWithPath = malloc(strlen(cfg.documentRootDirectory) + strlen(requestedFile) + 1);
		strcpy(fileWithPath, cfg.documentRootDirectory);
		strcat(fileWithPath, requestedFile);
		if (access(fileWithPath, F_OK) == -1) {
			statusCodes = malloc(strlen("404 Not Found") + 1);
			statusCodes = "404 Not Found";
			fileWithPath = malloc(strlen("/home/jan/2015-2016 Wintersemester/DV1457 H15 Lp1 Programmering in UNIX-miljö (campus)/Lab assignments/Lab assignment 2/project/www/error pages/404 Not Found.html") + 1);
			fileWithPath = "/home/jan/2015-2016 Wintersemester/DV1457 H15 Lp1 Programmering in UNIX-miljö (campus)/Lab assignments/Lab assignment 2/project/www/error pages/404 Not Found.html";
		} else {
			if (access(fileWithPath, R_OK) == -1) {
				statusCodes = malloc(strlen("403 Forbidden") + 1);
				statusCodes = "403 Forbidden";
				// load error page
			} else {
				if (strcmp(requestType, "HEAD") == 0 || strcmp(requestType, "GET") == 0) {
					statusCodes = malloc(strlen("200 OK") + 1);
					statusCodes = "200 OK";

				} else {
					statusCodes = malloc(strlen("501 Not Implemented") + 1);
					statusCodes = "501 Not Implemented";
					// load error page
				}
			}
		}
	} else {
		statusCodes = malloc(strlen("400 Bad Request") + 1);
		statusCodes = "400 Bad Request";
		// overwrite requestedFile
	}

	FILE * filePointer;
	long fileLength;
	char * fileBuffer;
	filePointer = fopen(fileWithPath, "rb");  // Open the file in binary mode
	fseek(filePointer, 0, SEEK_END);          // Jump to the end of the file
	fileLength = ftell(filePointer);             // Get the current byte offset in the file
	rewind(filePointer);                      // Jump back to the beginning of the file
	fileBuffer = (char *)malloc((fileLength + 1) * sizeof(char)); // Enough memory for file + \0
	fread(fileBuffer, fileLength, 1, filePointer); // Read in the entire file
	fclose(filePointer); // Close the file

	int contentLength = strlen(fileBuffer);

	char * lastModified;
	struct stat attrib;
	stat(fileWithPath, &attrib);
	lastModified = ctime(&(attrib.st_mtime));
	strtok(lastModified, "\n"); /* Remove new line(\n). */

	char *header;
	asprintf(&header, "HTTP/1.1 %s\r\n"
		"Server: Lap assignment 2 - Simple Web Server\r\n"
		"Last-Modified: %s\r\n"
		"Content-Length: %d\r\n"
		"Content-Type: text/html\r\n\r\n",
		statusCodes, lastModified, contentLength);
	send(clientSocketDescriptor, header, strlen(header) + 1, 0);

	if (strcmp(requestType, "GET") == 0) { // send error page not only for get request TODO
		printf("GET request!\n");// in logfile
		if (send(clientSocketDescriptor, fileBuffer, strlen(fileBuffer) + 1, 0) == -1){
			fprintf(stderr, "Failed to send.");
		}
	}
	close(clientSocketDescriptor);
	//free(receiveBuffer);
	//free(requestType);
	//free(requestedFile);
	//free(fileWithPath);//
}