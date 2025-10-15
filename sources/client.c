#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"
#include "parson.h"

char *cookie_admin = NULL;
char *cookie_user = NULL;
char *user_token = NULL;

// extragerea cookiurilor din mesaj
char *extract_cookie(char *response) {
    char *cookie_start = strstr(response, "Set-Cookie: ");
    if (cookie_start == NULL)
        return NULL;

    cookie_start += strlen("Set-Cookie: ");
    char *cookie_end = strstr(cookie_start, ";");
    if (cookie_end == NULL)
        return NULL;

    int len = cookie_end - cookie_start;
    char *cookie = calloc(len + 1, sizeof(char));
    strncpy(cookie, cookie_start, len);
    cookie[len] = '\0';

    return cookie;
}

// extragerea codului de status din mesaj
int extract_status_code(const char *response) {
    int status_code = 0;

    if (sscanf(response, "HTTP/1.1 %d", &status_code) == 1) {
        return status_code;
    }
    return -1;
}

// resetarea conexiunii (pentru cazul in care serverul o inchide fortat - dupa eroare)
int reset_connection(int sockfd) {
    close_connection(sockfd);
    return open_connection("63.32.125.183", 8081, AF_INET, SOCK_STREAM, 0);
}

// extragerea tokenului pentru acces in biblioteca
char *extract_token(char *response) {
    const char *token_key = "\"token\":\"";
    char *token_start = strstr(response, token_key);
    if (token_start == NULL)
        return NULL;

    token_start += strlen(token_key);

    char *token_end = strchr(token_start, '"');
    if (token_end == NULL)
        return NULL;

    int len = token_end - token_start;
    char *token = calloc(len + 1, sizeof(char));
    strncpy(token, token_start, len);
    token[len] = '\0';

    return token;
}

// afiseaza mesajul de eroare corespunzator codului si returneaza socketul pt noua conexiune
int print_error_message(int status_code, int sockfd) {

    switch(status_code) {
        case 400: {
            printf("ERROR: Format sau credentiale gresite\n");
            break;
        }
        case 401: {
            printf("ERROR: Nu esti autentificat\n");
            break;
        }
        case 403: {
            printf("ERROR: Nu ai permisiuni necesare\n");
            break;
        }
        case 404: {
            printf("ERROR: Resursa nu a fost gasita\n");
            break;
        }
        case 409: {
            printf("ERROR: Resursa exista deja\n");
            break;
        }
        case 500: {
            printf("ERROR: Serverul e nefunctional\n");
            break;
        }
        default: {
            printf("ERROR: Alta eroare necunoscuta\n");
            break;
        }
    }

    // pentru a fi siguri ca serverul nu a inchis conexiunea dupa eroare, o resetz
    return reset_connection(sockfd);
}

// partea comuna pt add_movie si add_collection
char *add_movie_collection(int id, int Cid, int sockfd) {
    
    char *message;
    char *response;

    // folosesc JSON pentru formatare
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);
    json_object_set_number(root_object, "id", id);

    char *json_body = json_serialize_to_string(root_value);
    json_value_free(root_value);

    // construiesc url
    char url[200];
    snprintf(url, sizeof(url), "/api/v1/tema/library/collections/%d/movies", Cid);

    // daca nu s-a primit token special trimit mesaj fara drepturi
    if (user_token == NULL) {
        message = compute_post_request("63.32.125.183", url,
                                        "application/json", &json_body, 1, NULL, 0, NULL, 0);
    } else {

        // altfel adaug drepturile cu token
        char *headers[1];
        char *header = "Authorization: Bearer";
        headers[0] = malloc(strlen(header) + strlen(user_token) + 2);
        sprintf(headers[0], "%s %s", header, user_token);
        message = compute_post_request("63.32.125.183", url,
                                        "application/json", &json_body, 1, headers, 1, NULL, 0);
    }

    json_free_serialized_string(json_body);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    if (!response)
        return "ERROR: Nu s-a primit raspuns de la server\n";

    // extrag codul de raspuns din headerul de la server
    int status_code = extract_status_code(response);
    switch(status_code) {
        case 200: {
            return "SUCCESS: Filmul a fost adaugat in colectie\n";
            break;
        }
        case 201: {
            return "SUCCESS: Filmul a fost adaugat in colectie\n";
            break;
        }
        case 400: {
            return "ERROR: Format gresit\n";
            break;
        }
        case 401: {
            return "ERROR: Nu esti autentificat\n";
            break;
        }
        case 403: {
            return "ERROR: Nu ai permisiuni necesare\n";
            break;
        }
        case 500: {
            return "ERROR: Serverul e nefunctional\n";
            break;
        }
        default: {
            return "ERROR: Orice alta eroare\n";
            break;
        }
    }
}

// trimite mesaj http pentru get_collection
char* get_collection(int sockfd, int id) {

    char *message;

    // construiesc url
    char url[200];
    snprintf(url, sizeof(url), "/api/v1/tema/library/collections/%d", id);

    // daca nu s-a primit token special trimit mesaj fara drepturi
    if (user_token == NULL) {
        message = compute_get_request("63.32.125.183", url,
                                        NULL, NULL, 0, NULL, 0);
    } else {

        // altfel adaug drepturile cu token
        char *headers[1];
        char *header = "Authorization: Bearer";
        headers[0] = malloc(strlen(header) + strlen(user_token) + 2);
        sprintf(headers[0], "%s %s", header, user_token);
        message = compute_get_request("63.32.125.183", url,
                                        NULL, headers, 1, NULL, 0);
    }

    send_to_server(sockfd, message);
    return receive_from_server(sockfd);
}

int main(int argc, char *argv[])
{
    char *message;
    char *response;
    int sockfd;

    // dechid conexiune cu server
    sockfd = open_connection("63.32.125.183", 8081, AF_INET, SOCK_STREAM, 0);

    // citesc de la tastatura comanda pe care vreau sa o fac
    char s[100];

    while (1) {
        scanf("%99s", s);

        if (strcmp(s, "login_admin") == 0) {

            // citesc credentiale
            char username[100], password[100];
            printf("username=");
            scanf("%99s", username);
            printf("password=");
            scanf("%99s", password);

            // restrictie pentru logari unice odata
            if(cookie_user != NULL || cookie_admin != NULL) {
                printf("ERROR: Esti deja logat intr-un cont\n");
                continue;
            }

            // folosesc JSON pentru formatare
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "username", username);
            json_object_set_string(root_object, "password", password);

            char *json_body = json_serialize_to_string(root_value);
            json_value_free(root_value);
            
            // trimit request de post spre server
            message = compute_post_request("63.32.125.183", "/api/v1/tema/admin/login",
                                            "application/json", &json_body, 1, NULL, 0, NULL, 0);
            json_free_serialized_string(json_body);

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {

                // retin cookie pentru autentificarea curenta
                printf("SUCCESS: Admin autentificat cu succes\n");
                char *new_cookie = extract_cookie(response);
                if (new_cookie != NULL) {
                    if (cookie_admin != NULL)
                        free(cookie_admin);
                    cookie_admin = new_cookie;
                }
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }
        }

        if (strcmp(s, "add_user") == 0) {

            // citesc credentiale noului utilizator
            char username[100], password[100];
            while (getchar() != '\n');
            do {
                printf("username=");
                fgets(username, sizeof(username), stdin);
                username[strcspn(username, "\n")] = 0;
            } while (strlen(username) == 0 || strspn(username, " \t") == strlen(username));
            do {
                printf("password=");
                fgets(password, sizeof(password), stdin);
                password[strcspn(password, "\n")] = 0;
            } while (strlen(password) == 0 || strspn(password, " \t") == strlen(password));

            // folosesc JSON pentru formatare
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "username", username);
            json_object_set_string(root_object, "password", password);

            char *json_body = json_serialize_to_string(root_value);
            json_value_free(root_value);

            // daca nu s-a logat adminul trimit mesaj fara drepturi
            if (cookie_admin == NULL) {
                message = compute_post_request("63.32.125.183", "/api/v1/tema/admin/users",
                                                "application/json", &json_body, 1, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile de admin
                char *cookies[1];
                cookies[0] = cookie_admin;
                message = compute_post_request("63.32.125.183", "/api/v1/tema/admin/users",
                                                "application/json", &json_body, 1, NULL, 0, cookies, 1);
            }
            json_free_serialized_string(json_body);

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {
                printf("SUCCESS: Utilizator adaugat\n");
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }
        }

        if (strcmp(s, "get_users") == 0) {

            // daca nu s-a logat adminul trimit mesaj fara drepturi
            if (cookie_admin == NULL) {
                message = compute_get_request("63.32.125.183", "/api/v1/tema/admin/users",
                                                NULL, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile de admin
                char *cookies[1];
                cookies[0] = cookie_admin;
                message = compute_get_request("63.32.125.183", "/api/v1/tema/admin/users",
                                                NULL, NULL, 0, cookies, 1);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {

                printf("SUCCESS: Urmatorii sunt utilizatorii:\n");
                char *body = basic_extract_json_response(response);

                if (body) {
                    JSON_Value *val = json_parse_string(body);
                    JSON_Object *obj = json_value_get_object(val);
                    JSON_Array *users = json_object_get_array(obj, "users");
                    size_t len = json_array_get_count(users);

                    for (int i = 0; i < len; i++) {
                        JSON_Object *u = json_array_get_object(users, i);
                        printf("#%d %s:%s\n", i, json_object_get_string(u, "username"),
                                            json_object_get_string(u, "password"));
                    }

                    json_value_free(val);
                }
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }
        }

        if (strcmp(s, "delete_user") == 0) {

            // citesc usernameul utilizatorului
            char username[100];
            printf("username=");
            scanf("%99s", username);

            // construiesc url
            char url[200];
            snprintf(url, sizeof(url), "/api/v1/tema/admin/users/%s", username);

            // daca nu s-a logat adminul trimit mesaj fara drepturi
            if (cookie_admin == NULL) {
                message = compute_delete_request("63.32.125.183", url,
                                                NULL, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile de admin
                char *cookies[1];
                cookies[0] = cookie_admin;
                message = compute_delete_request("63.32.125.183", url,
                                                NULL, NULL, 0, cookies, 1);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300)
                printf("SUCCESS: Utilizatorul a fost sters\n");
            else
                sockfd = print_error_message(status_code, sockfd);
        }

        if (strcmp(s, "login") == 0) {

            // citesc credentiale
            char admin[100], username[100], password[100];
            printf("admin_username=");
            scanf("%99s", admin);
            printf("username=");
            scanf("%99s", username);
            printf("password=");
            scanf("%99s", password);

            // restrictie pentru logari unice odata
            if(cookie_user != NULL || cookie_admin != NULL) {
                printf("ERROR: Esti deja logat intr-un cont\n");
                continue;
            }
            
            // folosesc JSON pentru formatare
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "admin_username", admin);
            json_object_set_string(root_object, "username", username);
            json_object_set_string(root_object, "password", password);

            char *json_body = json_serialize_to_string(root_value);
            json_value_free(root_value);
            
            // trimit request de post spre server
            message = compute_post_request("63.32.125.183", "/api/v1/tema/user/login",
                                            "application/json", &json_body, 1, NULL, 0, NULL, 0);
            json_free_serialized_string(json_body);

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {

                // retin cookie pentru autentificarea curenta
                printf("SUCCESS: Utilizator autentificat cu succes\n");
                char *new_cookie = extract_cookie(response);
                if (new_cookie != NULL) {
                    if (cookie_user != NULL)
                        free(cookie_user);
                    cookie_user = new_cookie;
                }
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }
        }

        if (strcmp(s, "get_access") == 0) {

            // daca nu s-a logat un user trimit mesaj fara drepturi
            if (cookie_user == NULL) {
                message = compute_get_request("63.32.125.183", "/api/v1/tema/library/access",
                                                NULL, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile de utilizator
                char *cookies[1];
                cookies[0] = cookie_user;
                message = compute_get_request("63.32.125.183", "/api/v1/tema/library/access",
                                                NULL, NULL, 0, cookies, 1);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {

                // retin cookie de token JWT pentru access
                printf("SUCCESS: Am primit token\n");
                char *token = extract_token(response);
                if (token != NULL) {
                    if (user_token != NULL)
                        free(user_token);
                    user_token = token;
                }
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }
        }

        if (strcmp(s, "get_movies") == 0) {

            // daca nu s-a primit token special trimit mesaj fara drepturi
            if (user_token == NULL) {
                message = compute_get_request("63.32.125.183", "/api/v1/tema/library/movies",
                                                NULL, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile cu token
                char *headers[1];
                char *header = "Authorization: Bearer";
                headers[0] = malloc(strlen(header) + strlen(user_token) + 2);
                sprintf(headers[0], "%s %s", header, user_token);
                message = compute_get_request("63.32.125.183", "/api/v1/tema/library/movies",
                                                NULL, headers, 1, NULL, 0);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {
                printf("SUCCESS: Lista filmelor\n");
                char *body = basic_extract_json_response(response);

                if (body) {
                    JSON_Value *val = json_parse_string(body);
                    JSON_Object *obj = json_value_get_object(val);
                    JSON_Array *movies = json_object_get_array(obj, "movies");
                    size_t len = json_array_get_count(movies);

                    for (int i = 0; i < len; i++) {
                        JSON_Object *u = json_array_get_object(movies, i);
                        printf("#%d %s\n", (int) json_object_get_number(u, "id"), json_object_get_string(u, "title"));
                    }

                    json_value_free(val);
                }
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }
        }

        if (strcmp(s, "get_movie") == 0) {

            // citesc id-ul filmului
            char id[10];
            printf("id=");
            scanf("%9s", id);

            // construiesc url
            char url[200];
            snprintf(url, sizeof(url), "/api/v1/tema/library/movies/%s", id);

            // daca nu s-a primit token special trimit mesaj fara drepturi
            if (user_token == NULL) {
                message = compute_get_request("63.32.125.183", url,
                                                NULL, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile cu token
                char *headers[1];
                char *header = "Authorization: Bearer";
                headers[0] = malloc(strlen(header) + strlen(user_token) + 2);
                sprintf(headers[0], "%s %s", header, user_token);
                message = compute_get_request("63.32.125.183", url,
                                                NULL, headers, 1, NULL, 0);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {
                printf("SUCCESS: Detalii film\n");
                char *body = basic_extract_json_response(response);

                if (body) {
                    JSON_Value *val = json_parse_string(body);
                    JSON_Object *u = json_value_get_object(val);
                    printf("title: %s\n", json_object_get_string(u, "title"));
                    printf("year: %d\n", (int) json_object_get_number(u, "year"));
                    printf("description: %s\n", json_object_get_string(u, "description"));
                    printf("rating: %.1f\n", atof(json_object_get_string(u, "rating")));
                    json_value_free(val);
                }
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }
        }

        if (strcmp(s, "add_movie") == 0) {

            // citesc detaliile noului film
            char title[100], year[10], description[100], rating[10];
            while (getchar() != '\n');
            do {
                printf("title=");
                fgets(title, sizeof(title), stdin);
                title[strcspn(title, "\n")] = 0;
            } while (strlen(title) == 0 || strspn(title, " \t") == strlen(title));
            int y;
            while (1) {
                printf("year=");
                fgets(year, sizeof(year), stdin);
                year[strcspn(year, "\n")] = 0;
                if (sscanf(year, "%d", &y) == 1)
                    break;
            }
            printf("description=");
            fgets(description, sizeof(description), stdin);
            description[strcspn(description, "\n")] = 0;
            float r;
            while (1) {
                printf("rating=");
                fgets(rating, sizeof(rating), stdin);
                rating[strcspn(rating, "\n")] = 0;
                if (sscanf(rating, "%f", &r) == 1)
                    break;
            }

            // folosesc JSON pentru formatare
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "title", title);
            json_object_set_number(root_object, "year", atoi(year));
            json_object_set_string(root_object, "description", description);
            json_object_set_number(root_object, "rating", atof(rating));

            char *json_body = json_serialize_to_string(root_value);
            json_value_free(root_value);

            // daca nu s-a primit token special trimit mesaj fara drepturi
            if (user_token == NULL) {
                message = compute_post_request("63.32.125.183", "/api/v1/tema/library/movies",
                                                "application/json", &json_body, 1, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile cu token
                char *headers[1];
                char *header = "Authorization: Bearer";
                headers[0] = malloc(strlen(header) + strlen(user_token) + 2);
                sprintf(headers[0], "%s %s", header, user_token);
                message = compute_post_request("63.32.125.183", "/api/v1/tema/library/movies",
                                                "application/json", &json_body, 1, headers, 1, NULL, 0);
            }

            json_free_serialized_string(json_body);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300)
                printf("SUCCESS: Filmul a fost adaugat cu succes\n");
            else
                sockfd = print_error_message(status_code, sockfd);
        }

        if (strcmp(s, "delete_movie") == 0) {
            
            // citesc id-ul filmului
            char id[10];
            printf("id=");
            scanf("%9s", id);

            // construiesc url
            char url[200];
            snprintf(url, sizeof(url), "/api/v1/tema/library/movies/%s", id);

            // daca nu s-a primit token special trimit mesaj fara drepturi
            if (user_token == NULL) {
                message = compute_delete_request("63.32.125.183", url,
                                                NULL, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile cu token
                char *headers[1];
                char *header = "Authorization: Bearer";
                headers[0] = malloc(strlen(header) + strlen(user_token) + 2);
                sprintf(headers[0], "%s %s", header, user_token);
                message = compute_delete_request("63.32.125.183", url,
                                                NULL, headers, 1, NULL, 0);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300)
                printf("SUCCESS: Filmul a fost sters\n");
            else
                sockfd = print_error_message(status_code, sockfd);
        }

        if (strcmp(s, "update_movie") == 0) {

            // citesc id-ul filmului
            char id[10];
            printf("id=");
            scanf("%9s", id);

            // citesc detaliile noului film
            char title[100], year[10], description[100], rating[10];
            while (getchar() != '\n');
            do {
                printf("title=");
                fgets(title, sizeof(title), stdin);
                title[strcspn(title, "\n")] = 0;
            } while (strlen(title) == 0 || strspn(title, " \t") == strlen(title));
            int y;
            while (1) {
                printf("year=");
                fgets(year, sizeof(year), stdin);
                year[strcspn(year, "\n")] = 0;
                if (sscanf(year, "%d", &y) == 1)
                    break;
            }
            printf("description=");
            fgets(description, sizeof(description), stdin);
            description[strcspn(description, "\n")] = 0;
            float r;
            while (1) {
                printf("rating=");
                fgets(rating, sizeof(rating), stdin);
                rating[strcspn(rating, "\n")] = 0;
                if (sscanf(rating, "%f", &r) == 1)
                    break;
            }

            // folosesc JSON pentru formatare
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "title", title);
            json_object_set_number(root_object, "year", atoi(year));
            json_object_set_string(root_object, "description", description);
            json_object_set_number(root_object, "rating", atof(rating));

            char *json_body = json_serialize_to_string(root_value);
            json_value_free(root_value);

            // construiesc url
            char url[200];
            snprintf(url, sizeof(url), "/api/v1/tema/library/movies/%s", id);

            // daca nu s-a primit token special trimit mesaj fara drepturi
            if (user_token == NULL) {
                message = compute_put_request("63.32.125.183", url,
                                                "application/json", &json_body, 1, NULL, 0);
            } else {

                // altfel adaug drepturile cu token
                char *headers[1];
                char *header = "Authorization: Bearer";
                headers[0] = malloc(strlen(header) + strlen(user_token) + 2);
                sprintf(headers[0], "%s %s", header, user_token);
                message = compute_put_request("63.32.125.183", url,
                                                "application/json", &json_body, 1, headers, 1);
            }

            json_free_serialized_string(json_body);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300)
                printf("SUCCESS: Filmul a fost modificat cu succes\n");
            else
                sockfd = print_error_message(status_code, sockfd);
        }

        if (strcmp(s, "get_collections") == 0) {

            // daca nu s-a primit token special trimit mesaj fara drepturi
            if (user_token == NULL) {
                message = compute_get_request("63.32.125.183", "/api/v1/tema/library/collections",
                                                NULL, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile cu token
                char *headers[1];
                char *header = "Authorization: Bearer";
                headers[0] = malloc(strlen(header) + strlen(user_token) + 2);
                sprintf(headers[0], "%s %s", header, user_token);
                message = compute_get_request("63.32.125.183", "/api/v1/tema/library/collections",
                                                NULL, headers, 1, NULL, 0);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {
                printf("SUCCESS: Lista colectiilor\n");
                char *body = basic_extract_json_response(response);

                if (body) {
                    JSON_Value *val = json_parse_string(body);
                    JSON_Object *obj = json_value_get_object(val);
                    JSON_Array *collections = json_object_get_array(obj, "collections");
                    size_t len = json_array_get_count(collections);

                    for (int i = 0; i < len; i++) {
                        JSON_Object *u = json_array_get_object(collections, i);
                        printf("#%d: %s\n", (int) json_object_get_number(u, "id"), json_object_get_string(u, "title"));
                    }

                    json_value_free(val);
                }
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }                    
        }

        if (strcmp(s, "get_collection") == 0) {

            // citesc id-ul colectiei
            char id[10];
            printf("id=");
            scanf("%9s", id);

            response = get_collection(sockfd, atoi(id));
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {
                printf("SUCCESS: Detalii colectie\n");
                char *body = basic_extract_json_response(response);

                if (body) {
                    JSON_Value *val = json_parse_string(body);
                    JSON_Object *u = json_value_get_object(val);
                    printf("title: %s\n", json_object_get_string(u, "title"));
                    printf("owner: %s\n", json_object_get_string(u, "owner"));

                    JSON_Array *movies = json_object_get_array(u, "movies");
                    size_t len = json_array_get_count(movies);

                    for (int i = 0; i < len; i++) {
                        JSON_Object *movie = json_array_get_object(movies, i);
                        printf("#%d: %s\n", (int) json_object_get_number(movie, "id"),
                                            json_object_get_string(movie, "title"));
                    }
                    json_value_free(val);
                }
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }
        }

        if (strcmp(s, "add_collection") == 0) {

            // citesc detaliile noului film
            char title[100], num[10], id[100][10];
            while (getchar() != '\n');
            do {
                printf("title=");
                fgets(title, sizeof(title), stdin);
                title[strcspn(title, "\n")] = 0;
            } while (strlen(title) == 0 || strspn(title, " \t") == strlen(title));
            int n;
            while (1) {
                printf("num_movies=");
                fgets(num, sizeof(num), stdin);
                num[strcspn(num, "\n")] = 0;
                if (sscanf(num, "%d", &n) == 1)
                    break;
            }
            for (int i = 0; i < atoi(num); i++) {
                printf("movie_id[%d]=", i);
                fgets(id[i], sizeof(id[i]), stdin);
                id[i][strcspn(id[i], "\n")] = 0;
            }

            // folosesc JSON pentru formatare
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "title", title);

            char *json_body = json_serialize_to_string(root_value);
            json_value_free(root_value);

            // daca nu s-a primit token special trimit mesaj fara drepturi
            if (user_token == NULL) {
                message = compute_post_request("63.32.125.183", "/api/v1/tema/library/collections",
                                                "application/json", &json_body, 1, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile cu token
                char *headers[1];
                char *header = "Authorization: Bearer";
                headers[0] = malloc(strlen(header) + strlen(user_token) + 2);
                sprintf(headers[0], "%s %s", header, user_token);
                message = compute_post_request("63.32.125.183", "/api/v1/tema/library/collections",
                                                "application/json", &json_body, 1, headers, 1, NULL, 0);
            }

            json_free_serialized_string(json_body);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {
                char *json_start = strstr(response, "\r\n\r\n");
                json_start += 4;
                JSON_Value *value = json_parse_string(json_start);
                JSON_Object *obj = json_value_get_object(value);
                int Cid = (int)json_object_get_number(obj, "id");
                json_value_free(value);
                int ok = 1;

                for (int i = 0; i < atoi(num); i++) {
                    if(strncmp(add_movie_collection(atoi(id[i]), Cid, sockfd),"ERROR", 5) == 0) {
                        printf("ERROR: Filmul cu id-ul %d nu a putut fi adaugat\n", atoi(id[i]));
                        sockfd = reset_connection(sockfd);
                        ok = 0;
                        break;
                    }
                }
                if (ok == 1)
                    printf("SUCCESS: Colectia a fost adaugata cu succes\n");
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }
        }

        if (strcmp(s, "delete_collection") == 0) {

            // citesc id-ul colectiei
            char id[10];
            printf("id=");
            scanf("%9s", id);

            // construiesc url
            char url[200];
            snprintf(url, sizeof(url), "/api/v1/tema/library/collections/%s", id);

            // daca nu s-a primit token special trimit mesaj fara drepturi
            if (user_token == NULL) {
                message = compute_delete_request("63.32.125.183", url,
                                                NULL, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile cu token
                char *headers[1];
                char *header = "Authorization: Bearer";
                headers[0] = malloc(strlen(header) + strlen(user_token) + 2);
                sprintf(headers[0], "%s %s", header, user_token);
                message = compute_delete_request("63.32.125.183", url,
                                                NULL, headers, 1, NULL, 0);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300)
                printf("SUCCESS: Colectia a fost stearsa\n");
            else
                sockfd = print_error_message(status_code, sockfd);
        }

        if (strcmp(s, "add_movie_to_collection") == 0) {

            // citesc detaliile noului film
            int Cid, id;
            printf("collection_id=");
            scanf("%d", &Cid);

            printf("movie_id=");
            scanf("%d", &id);

            // adaug filmul
            char *sir = add_movie_collection(id, Cid, sockfd);
            if(strncmp(sir,"ERROR", 5) == 0) {
                printf("%s", sir);
                sockfd = reset_connection(sockfd);
            } else {
                printf("%s", sir);
            }
        }

        if (strcmp(s, "delete_movie_from_collection") == 0) {

            // citesc detaliile colectiei si a filmului
            int Cid, id;
            printf("collection_id=");
            scanf("%d", &Cid);

            printf("movie_id=");
            scanf("%d", &id);

            // construiesc url
            char url[200];
            snprintf(url, sizeof(url), "/api/v1/tema/library/collections/%d/movies/%d", Cid, id);

            // daca nu s-a primit token special trimit mesaj fara drepturi
            if (user_token == NULL) {
                message = compute_delete_request("63.32.125.183", url,
                                                NULL, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile cu token
                char *headers[1];
                char *header = "Authorization: Bearer";
                headers[0] = malloc(strlen(header) + strlen(user_token) + 2);
                sprintf(headers[0], "%s %s", header, user_token);
                message = compute_delete_request("63.32.125.183", url,
                                                NULL, headers, 1, NULL, 0);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) 
                printf("SUCCESS: Filmul a fost sters din colectie\n");
            else
                sockfd = print_error_message(status_code, sockfd);
        }

        if (strcmp(s, "logout_admin") == 0) {

            // daca nu s-a logat adminul trimit mesaj fara drepturi
            if (cookie_admin == NULL) {
                message = compute_get_request("63.32.125.183", "/api/v1/tema/admin/logout",
                                                NULL, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile de admin
                char *cookies[1];
                cookies[0] = cookie_admin;
                message = compute_get_request("63.32.125.183", "/api/v1/tema/admin/logout",
                                                NULL, NULL, 0, cookies, 1);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {
                printf("SUCCESS: Admin delogat\n");
                free(cookie_admin);
                cookie_admin = NULL;
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }
        }

        if (strcmp(s, "logout") == 0) {

            // daca nu s-a logat utilizatorul trimit mesaj fara drepturi
            if (cookie_user == NULL) {
                message = compute_get_request("63.32.125.183", "/api/v1/tema/user/logout",
                                                NULL, NULL, 0, NULL, 0);
            } else {

                // altfel adaug drepturile de utilizator
                char *cookies[1];
                cookies[0] = cookie_user;
                message = compute_get_request("63.32.125.183", "/api/v1/tema/user/logout",
                                                NULL, NULL, 0, cookies, 1);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (!response) {
                printf("ERROR: Nu s-a primit raspuns de la server\n");
                sockfd = reset_connection(sockfd);
                continue;
            }

            // extrag codul de raspuns din headerul de la server
            int status_code = extract_status_code(response);
            if (status_code >= 200 && status_code < 300) {

                // retin cookie pentru autentificarea curenta
                printf("SUCCESS: Utilizator delogat\n");
                free(cookie_user);
                cookie_user = NULL;
                if (user_token != NULL) {
                    free(user_token);
                    user_token = NULL;
                }
            } else {
                sockfd = print_error_message(status_code, sockfd);
            }
        }

        if (strcmp(s, "exit") == 0) {
            break;
        }
    }

    // inchid conexiunea cu serverul
    close_connection(sockfd);
    
    return 0;
}