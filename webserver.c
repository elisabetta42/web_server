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

#include <signal.h>
#include <sys/resource.h>

#define BUFFERSIZE 1024
/* define quene lenght? */
typedef enum { false, true } bool;
//create a structure to store the configuration settings
typedef struct configuration {
	char* logfile;
	int port;
	bool daemon;
	char* requestHandlingMethod;
	char* documentRootDirectory;

} configuration;

configuration readConfigurationFile(configuration cfg);

main(int argc, char* argv[])
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
				//handling parameter with logfile name
				int valueSize = strlen(argv[i]);
				cfg.logfile = malloc(valueSize);//allocate memory for the exact lenght of the string file name given as parameter
				strcpy(cfg.logfile, argv[i]);
			} else {
				fprintf(stderr, "No logfile specified, logging will be output to syslog.\n");
			}
		} else if (strcmp(argv[i], "-s") == 0) { /* Set request handling method.*/
			if (i + 1 <= argc - 1) {
				i++;
				//same mechanism ...
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
	fprintf(stdout, "Document root directory: %s\n", cfg.documentRootDirectory);// /n to much(reason?)

	/*create socket */
	if((serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Failed to create socket.\n");
		exit(-1);
	}
	memset(&serverAddress, '0', sizeof(serverAddress)); /*overwrites serverAddress with zeros?*/

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
		fprintf(stderr, "Failed to start listing.");
		exit(-1);
	}
	fprintf(stdout, "Webserver started.\n");
	if (cfg.daemon) {
		runAsDaemon();
	}

	// /from this point no outputs to stdout all to logfile
	int clientSocketDescriptor = 0;
	while(1) {
		/* create new (client) socket */
		if ((clientSocketDescriptor = accept(serverSocketDescriptor, (struct sockaddr*)NULL, NULL)) == -1) {
			fprintf(stderr, "Failed to establish a connection.");
		}
		handleRequest(clientSocketDescriptor, cfg.documentRootDirectory);
        // free auf alle pointer
        //sleep(1);
     }
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
	char *delimiter = ":";
	while ((read = getline(&line, &len, fp)) != -1) {
		char *value;
		char *key = strtok_r(line, delimiter, &value);
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

runAsDaemon(const char *cmd) {
	pid_t processId = 0;
	struct rlimit maxFileDescriptors;
	struct sigaction signalAction;

	umask(0);
	if (getrlimit(RLIMIT_NOFILE, &maxFileDescriptors) < 0) {
        fprintf(stderr, "Failed to get maximum number of file descriptors.");
        exit(-1);
    }
	if ((processId = fork()) < 0) {
		fprintf(stderr, "Failed to create child process.");
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
        fprintf(stderr, "Failed to ignore SIGHUP");
        exit(-1);
    }
    if ((processId = fork()) < 0){
        perror("Can't fork");
        exit(1);
    } else if (processId != 0) {/* parent */
        exit(0);
    }
	if (chdir("/") < 0){
        fprintf(stderr, "Failed to change current working directory to root.");
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

handleRequest(int clientSocketDescriptor, char * documentRootDirectory) {
	char receiveBuffer[BUFFERSIZE];
	memset(receiveBuffer, '0', sizeof(receiveBuffer)); /*overwrites receiveBuffer with zeros?*///needed?
	printf("Connection happened\n");// in logfile

	if (recv(clientSocketDescriptor, receiveBuffer, sizeof(receiveBuffer), 0) == -1) {
		fprintf(stderr, "Failed to receive.");
	}
	printf("%s###############",receiveBuffer);
	char * statusCodes;
	char * requestedFile;
	//printf("%s", receiveBuffer);

	char *delimiterLine = "\n";
	char *delimiter = " ";
	char * token;
	char * line;
	while ((line = strtok(receiveBuffer, delimiterLine)) != NULL) {
		printf("##%s",line);
		while ((token = strtok(line, delimiter)) != NULL) {
			if (strcmp(token, "HEAD") == 0) {

			} else if (strcmp(token, "GET") == 0){
				requestedFile = strtok(NULL, delimiter);
				//printf("%s", requestedFile);
			}
		}
	}

	/*
    if (strncmp(receiveBuffer, "HEAD", 4) == 0 || strncmp(receiveBuffer, "GET", 3) == 0) {
        int bufferLenght = 0;
        char *buffer;
		if (strncmp(receiveBuffer, "GET", 3) == 0) {
        	FILE *fileptr;
			long filelen;
			fileptr = fopen("index.html", "rb");  // Open the file in binary mode
			fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
			filelen = ftell(fileptr);             // Get the current byte offset in the file
			rewind(fileptr);                      // Jump back to the beginning of the file
			buffer = (char *)malloc((filelen+1)*sizeof(char)); // Enough memory for file + \0
			fread(buffer, filelen, 1, fileptr); // Read in the entire file
			fclose(fileptr); // Close the file
			bufferLenght = strlen(buffer);				
        }
        printf("HEAD request!\n");    // in logfile    	

		char *bHeader;
		asprintf(&bHeader, "HTTP/1.1 200 Ok\r\nServer: Atasoy Simple Web Server\r\nContent-Length: %d\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n", bufferLenght);
	    send(clientSocketDescriptor, bHeader, strlen(bHeader) + 1, 0);

        if (strncmp(receiveBuffer, "GET", 3) == 0) {
        	printf("GET request!\n");// in logfile
        	if (send(clientSocketDescriptor, buffer, strlen(buffer) + 1, 0) == -1){
        		fprintf(stderr, "Failed to send.");
        	}
        }
    } else {
    	/* invalid http method type -> return 501*/
    //}*/
    close(clientSocketDescriptor);
}