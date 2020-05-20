/**
 * webserver.c -- A webserver written in C
 * 
 * Test with curl (if you don't have it, install it):
 * 
 *    curl -D - http://localhost:4500/
 *    curl -D - http://localhost:4500/d20
 *    curl -D - http://localhost:4500/date
 * 
 * You can also test the above URLs in your browser! They should work!
 * 
 * Posting Data:
 * 
 *    curl -D - -X POST -H 'Content-Type: text/plain' -d 'Hello, sample data!' http://localhost:4500/save
 * 
 * (Posting data is harder to test from a browser.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include "net.h"
#include "file.h"
#include "mime.h"
#include "cache.h"

#define MAXLINE 4096

#define PORT "4500"  // the port users will be connecting to

#define SERVER_FILES "./serverfiles"
#define SERVER_ROOT "./serverroot"


/**
 * Send an HTTP response
 *
 * header:       "HTTP/1.1 404 NOT FOUND" or "HTTP/1.1 200 OK", etc.
 * content_type: "text/plain", etc.
 * body:         the data to send.
 * 
 * Return the value from the send() function.
 */
int send_response(int fd, char *header, char *content_type, void *body, int content_length)
{
    const int max_response_size = 262144;
    char response[max_response_size];
    int response_length = 0, read = 0;
    char *rp = response;
    time_t rawtime;
    struct tm *timeinfo;
    
    // Get the current time
    time( &rawtime );
    timeinfo = localtime( &rawtime );

    // Build HTTP response and store it in response
    //
    // Build header
    read = sprintf(rp, "%s\n", header);
    response_length += read;
    rp += read;

    // Build Date
    read = sprintf(rp, "Date: %s", asctime(timeinfo));
    response_length += read;
    rp += read;

    // Build Content_Length
    read = sprintf(rp, "Content-Length: %d\n", content_length);
    response_length += read;
    rp += read;

    // Build Conten-Type
    read = sprintf(rp, "Content-Type: %s\n\n", content_type);
    response_length += read;
    rp += read;
    
    // Build Content
    memcpy(rp, body, content_length);
    response_length += content_length;
    rp += content_length;
    
    // Handle the end of response string
    *(rp++) = '\0';
    response_length++;

    // Send it all!
    int rv = send(fd, response, response_length, 0);

    if (rv < 0) {
        perror("send");
    }

    return rv;
}


/**
 * Send a /d20 endpoint response
 */
void get_d20(int fd)
{
    // Generate a random number between 1 and 20 inclusive
    int r = rand() % 20 + 1;

    // Use send_response() to send it back as text/plain data
    char header[MAXLINE] = "HTTP/1.1 200 OK";
    char content_type[MAXLINE] = "text/plain";
    char body[MAXLINE];
    sprintf(body, "you get a random number: %d", r);
    int content_length = strlen(body);
    
    // Send response to client
    send_response(fd, header, content_type, body, content_length);
}

/**
 * Send a 404 response
 */
void resp_404(int fd)
{
    char filepath[MAXLINE];
    struct file_data *filedata; 
    char *mime_type;

    // Fetch the 404.html file
    snprintf(filepath, sizeof filepath, "%s/404.html", SERVER_FILES);
    filedata = file_load(filepath);

    if (filedata == NULL) {
        // TODO: make this non-fatal
        fprintf(stderr, "cannot find system 404 file\n");
        exit(3);
    }

    mime_type = mime_type_get(filepath);

    send_response(fd, "HTTP/1.1 404 NOT FOUND", mime_type, filedata->data, filedata->size);

    file_free(filedata);
}

/**
 * Read and return a file from disk or cache
 */
void get_file(int fd, struct cache *cache, char *request_path)
{
    char filepath[MAXLINE];
    struct file_data *filedata;
    char *mime_type;
    struct cache_entry *ce;

    // Build the path of request file
    // If it's '/'
    if (strcmp(request_path, "/") == 0) {
        // Redirect to index.html
        sprintf(filepath, "%s%sindex.html", SERVER_ROOT, request_path);
    } else {
        sprintf(filepath, "%s%s", SERVER_ROOT, request_path);
    }
    
    // First check to see if the path to the file is in the cache 
    ce = cache_get(cache, filepath);

    // If it's in the cache
    if (ce != NULL) {
        // Serve it
        send_response(fd, "HTTP/1.1 200 OK", ce->content_type, ce->content, ce->content_length);
    }
    // If it's not in the cache
    else {
        // Load the file from disk
        filedata = file_load(filepath);
        mime_type = mime_type_get(filepath);

        // If it's in the disk
        if (filedata != NULL) {
            // Store the file in the cache
            cache_put(cache, filepath, mime_type, filedata->data, filedata->size);
            // Serve it
            send_response(fd, "HTTP/1.1 200 OK", mime_type, filedata->data, filedata->size);
            //
            file_free(filedata);
        } 
        // If it's not in the disk
        else {
            resp_404(fd);
        }
    }
}

/**
 * Search for the end of the HTTP header
 * 
 * "Newlines" in HTTP can be \r\n (carriage return followed by newline) or \n
 * (newline) or \r (carriage return).
 */
char *find_start_of_body(char *header)
{
    char *ret;

    if ( (ret = strstr(header, "\r\n\r\n")) != NULL) {
        ret += 4;
        return ret;
    } else if ( (ret = strstr(header, "\r\r")) != NULL) {
        ret += 2;
        return ret;
    } else if ( (ret = strstr(header, "\n\n")) != NULL) {
        ret += 2;
        return ret;
    } else {
        return NULL;
    }
}

/*
 *
 * Post a file
 *
 */
void post_save(int fd, char *save_path, char *save_content, int body_length) {
    FILE *fp;
    char filepath[MAXLINE];

    // Build the path of saved file
    sprintf(filepath, "%s%s/data", SERVER_ROOT, save_path); 

    // Save file content
    // Open file
    if ((fp = fopen(filepath, "w")) == NULL) {
        fprintf(stderr, "save path error\n");
        return;
    }
    // Write file
    fwrite(save_content, 1, body_length, fp);
    // Close file
    fclose(fp);

    // Serve it
    send_response(fd, "HTTP/1.1 200 OK", "application/json", "{\"status\":\"OK\"}", 15);
}

/**
 *
 * Handle HTTP request and send response
 *
 */
void handle_http_request(int fd, struct cache *cache)
{
    const int request_buffer_size = 65536; // 64K
    char request[request_buffer_size];
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE], body[request_buffer_size];

    // Read request
    int bytes_recvd = recv(fd, request, request_buffer_size - 1, 0);
    request[strlen(request)] = '\0';

    if (bytes_recvd < 0) {
        perror("recv");
        return;
    }

    // Read the three components of the first line of the request 
    sscanf(request, "%s %s %s\n", method, uri, version);

    // Read body in order to handling the post request
    char *start_of_body = find_start_of_body(request);
    int body_length = bytes_recvd - (start_of_body - request);

    if (start_of_body != NULL) memcpy(body, start_of_body, body_length);

    // If GET, handle the get endpoints
    if (strcasecmp(method, "get") == 0) {
        // Check if it's /d20 and handle that special case
        if (strcmp(uri, "/d20") == 0) {
            get_d20(fd);
        } 
        // Otherwise serve the requested file by calling get_file()
        else {
            get_file(fd, cache, uri);
        }
    } 
    else if (strcasecmp(method, "post") == 0) {
        if (start_of_body == NULL) return;
        post_save(fd, uri, body, body_length);
    }
    else {
        send_response(fd, "HTTP/1.1 501 Not Implemented", "text/plain", NULL, 0);
    }
}

/**
 * Main
 */
int main(void)
{
    int newfd;  // listen on sock_fd, new connection on newfd
    struct sockaddr_storage their_addr; // connector's address information
    char s[INET6_ADDRSTRLEN];

    struct cache *cache = cache_create(10, 0);

    srand(time(NULL));

    // Get a listening socket
    int listenfd = get_listener_socket(PORT);

    if (listenfd < 0) {
        fprintf(stderr, "webserver: fatal error getting listening socket\n");
        exit(1);
    }

    printf("webserver: waiting for connections on port %s...\n", PORT);

    // This is the main loop that accepts incoming connections and
    // responds to the request. The main parent process
    // then goes back to waiting for new connections.
    
    while(1) {
        socklen_t sin_size = sizeof their_addr;

        // Parent process will block on the accept() call until someone
        // makes a new connection:
        newfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);
        if (newfd == -1) {
            perror("accept error");
            continue;
        }

        // Print out a message that we got the connection
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);
        
        // newfd is a new socket descriptor for the new connection.
        // listenfd is still listening for new connections.

        handle_http_request(newfd, cache);

        close(newfd);
    }

    // Unreachable code

    return 0;
}

