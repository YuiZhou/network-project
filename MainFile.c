/* To compile: gcc sircd.c rtlib.c rtgrading.c csapp.c -lpthread -osircd */
#include "rtlib.h"
#include "rtgrading.h"
#include "csapp.h"
#include <stdlib.h>
#include <stdarg.h>


/* Macros */
#define MAX_MSG_TOKENS 10
#define MAX_MSG_LEN 512

#define MAX_NAME_LEN 64
#define ANONYMOUS "ANONYMOUS"

typedef struct { /* represents a pool of connected descriptors */
    int maxfd;        /* largest descriptor in read_set */   
    fd_set read_set;  /* set of all active descriptors */
    fd_set ready_set; /* subset of descriptors ready for reading  */
    int nready;       /* number of ready descriptors from select */   
    int maxi;         /* highwater index into client array */
    int clientfd[FD_SETSIZE];    /* set of active descriptors */
    rio_t clientrio[FD_SETSIZE]; /* set of active read buffers */
} pool;

typedef struct {
    char hostname[MAX_NAME_LEN];
    char realname[MAX_NAME_LEN];
    char username[MAX_NAME_LEN];
    char nickname[MAX_NAME_LEN];
    int fd; /* user uses this fd for IO */
    size_t index; /* the user's index in user_list which is equal to the index of fd in clientfd array */
    int channel; /* index of user's following channel, -1 if null */
} user;

typedef struct {
    char channelname[MAX_NAME_LEN];
    size_t index; /* the channel's index in channel_list */
    int follower[FD_SETSIZE]; /* a set of follower's index */
} channel;

/* Global variables */
u_long curr_nodeID;
rt_config_file_t   curr_node_config_file;  /* The config_file  for this node */
rt_config_entry_t *curr_node_config_entry; /* The config_entry for this node */

pool *p;
user* user_list[FD_SETSIZE];
channel* channel_list[FD_SETSIZE];


/* Function prototypes */
void init_node( int argc, char *argv[] );
size_t get_msg( char *buf, char *msg );
int tokenize( char const *in_buf, char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN+1], char delim );

void init_pool(int listenfd);
void add_client(int connfd);
void check_clients();
void parse_cmd(int fd, char *msg, size_t n);
void reply(int fd, char *msg, ...);
int find_channel_by_name(channel **c, char *channelname);
int find_user_by_fd(user **u, int fd);
int find_user_by_nick(user **u, char *nickname);
void handle_nick(int fd, char *nickname);
void handle_user(int fd, char *username, char *hostname, char *realname);
void handle_quit(int fd);
void handle_join(int fd, char *channelname);
void handle_who(int fd, char *channelname);
void handle_list(int fd);
void handle_privmsg(int fd, char *to_nick, char *msg);
void handle_part(int fd);

/* Main */
int main( int argc, char *argv[] )
{
    
    int listenfd, connfd; 
    socklen_t clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;

    init_node( argc, argv );
    printf( "I am node %lu and I listen on port %d for new users\n", curr_nodeID, curr_node_config_entry->irc_port );

    listenfd = Open_listenfd(curr_node_config_entry->irc_port);
    init_pool(listenfd); //line:conc:echoservers:initpool
    while (1) {
        /* Wait for listening/connected descriptor(s) to become ready */
        p -> ready_set = p -> read_set;
        p -> nready = Select(p -> maxfd+1, &(p -> ready_set), NULL, NULL, NULL);

        /* If listening descriptor ready, add new client to pool */
        if (FD_ISSET(listenfd, &(p -> ready_set))) { //line:conc:echoservers:listenfdready
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:conc:echoservers:accept
            add_client(connfd); //line:conc:echoservers:addclient
        }
        
        /* Echo a text line from each ready connected descriptor */ 
        check_clients(); //line:conc:echoservers:checkclients
    }
    return 0;
}

/*
 * init_pool ( int listenfd )
 *
 * int listenfd : the server's file discriptor
 * 
 * Initializes the pool of active clients.
 */
/* $begin init_pool */
void init_pool(int listenfd) 
{
    /* Initially, there are no connected descriptors */
    int i;

    p = (pool *)Malloc(sizeof(pool));
    p->maxi = -1;                   //line:conc:echoservers:beginempty
    for (i=0; i< FD_SETSIZE; i++)  
        p->clientfd[i] = -1;        //line:conc:echoservers:endempty

    /* Initially, listenfd is only member of select read set */
    p->maxfd = listenfd;            //line:conc:echoservers:begininit
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set); //line:conc:echoservers:endinit
}
/* $end init_pool */

/*
 * init_user ( size_t id, int fd )
 *
 * size_t id : the id of the user in the pool
 * int fd : the user's file discriptor
 *
 * Initiallizes a user and add it to the user list.
 * All its name is set to ANONYMOUS, and belong to none channel (-1).
 */
void init_user(size_t id, int fd){
    user *u = (user *)Malloc(sizeof(user));
    strcpy(u -> hostname, ANONYMOUS);
    strcpy(u -> realname, ANONYMOUS);
    strcpy(u -> username, ANONYMOUS);
    strcpy(u -> nickname, ANONYMOUS);
    u -> fd = fd;
    u -> index = id;
    u -> channel = -1;

    /* Add user to the user list at target place */
    user_list[id] = u;
}

/*
 * add_client ( int connfd )
 *
 * int connfd : the client's file discriptor
 * 
 * Adds a new client connection to the pool and initiliaze the client.
 */
/* $begin add_client */
void add_client(int connfd) 
{
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++)  /* Find an available slot */
        if (p->clientfd[i] < 0) { 
            /* Add connected descriptor to the pool */
            p->clientfd[i] = connfd;                 //line:conc:echoservers:beginaddclient
            Rio_readinitb(&p->clientrio[i], connfd); //line:conc:echoservers:endaddclient

            /* Add the descriptor to descriptor set */
            FD_SET(connfd, &p->read_set); //line:conc:echoservers:addconnfd

            init_user(i, connfd);

            /* Update max descriptor and pool highwater mark */
            if (connfd > p->maxfd) //line:conc:echoservers:beginmaxfd
                p->maxfd = connfd; //line:conc:echoservers:endmaxfd
            if (i > p->maxi)       //line:conc:echoservers:beginmaxi
                p->maxi = i;       //line:conc:echoservers:endmaxi
            break;
        }
    if (i == FD_SETSIZE) /* Couldn't find an empty slot */
        app_error("add_client error: Too many clients");
}
/* $end add_client */

/*
 * check_clients ( )
 * 
 * Services ready client connections.
 */
/* $begin check_clients */
void check_clients() 
{
    int i, connfd, n;
    char buf[MAXLINE]; 
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        /* If the descriptor is ready, echo a text line from it */
        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) { 
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) > 0) {
                parse_cmd(connfd, buf, n);
            }

            /* EOF detected, remove descriptor from pool */
            else {
                handle_quit(connfd);
            }
        }
    }
}
/* $end check_clients */

/*
 * parse_cmd( int fd, char *msg, size_t n )
 * 
 * int fd : file discriptor of the caller
 * char *msg : message the caller send
 * size_t n : size of msg
 *
 * parse the message, if it is a command, invoke the corresponding method.
 * Else transfer it to all users.
 */
/* $begin parse_cmd */
void parse_cmd(int fd, char *msg, size_t n){
    int i, connfd, argc;
    char argv[MAX_MSG_TOKENS][MAX_MSG_LEN+1];

    if(!(argc = tokenize(msg, argv, ' ')))
        return;

    get_msg(argv[0], argv[0]);
    
    /* compare the argv[0] and invoke the corresponding method */
    if( !strcmp(argv[0],"NICK") ){
        if( argc < 2 )
            return reply(fd, "Usage: %s <nickname>\r\n", argv[0]);
        return handle_nick(fd, argv[1]);
    }else if( !strcmp(argv[0], "USER") ){
        if( argc < 4 )
            return reply(fd, "Usage: %s <username> <hostname> <realname>\r\n", argv[0]);
        return handle_user(fd, argv[1], argv[2], argv[3]);
    }else if( !strcmp(argv[0], "JOIN") ){
        if( argc < 2 )
            return reply(fd, "Usage: %s <channel>\r\n", argv[0]);
        return handle_join(fd, argv[1]);
    }else if( !strcmp(argv[0], "WHO") ){
        if( argc < 2 )
            return reply(fd, "Usage: %s <channel>\r\n", argv[0]);
        return handle_who(fd, argv[1]);
    }else if( !strcmp(argv[0], "LIST") ){
        return handle_list(fd);
    }else if( !strcmp(argv[0], "PRIVMSG") ){
        if( argc < 3 )
            return reply(fd, "Usage: %s <to> <message>\r\n", argv[0]);
        return handle_privmsg(fd, argv[1], argv[2]);
    }else if( !strcmp(argv[0], "PART") ){
        return handle_part(fd);
    }

    /* transfer the msg to every user */
    for (i = 0; i <= p->maxi; i++) {
        connfd = p->clientfd[i];
        /* it is not an empty slot */
        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set)))
            Rio_writen(connfd, msg, n);
    } 
}
/* $end parse_cmd */


 /*
  * reply ( int fd, const char* msg, ... )
  * 
  * int fd : user's file discriptor who need to replied.
  * const char *msg : the message should to transferred
  * 
  * transfer a message to a target fd.
  */
void reply(int fd, char *msg, ...){
    char rep[MAXLINE];
    memset(rep, '\0', MAXLINE);

    va_list args;
    va_start(args, msg);
    vsprintf(rep, msg, args);
    Rio_writen(fd, rep, strlen(rep));
}

/***************
 * Find methods
 ***************/
/*
 * find_channel_by_name ( channel **c, char *channelname )
 * 
 * channel **c : store the result
 * char *channelname : target channelname
 *
 * find a channel in the channel list with given channelname,
 * and store the channel in c.
 *
 * Returns the index of channel in channel list, -1 if not find 
 */
 int find_channel_by_name(channel **c, char *channelname){
    int i;
    channel *tmp_c;

    for(i = 0; i < FD_SETSIZE; i++){
        tmp_c = channel_list[i];
        if(tmp_c && !strcmp(channelname, tmp_c -> channelname)){
            *c = tmp_c;
            return i;
        }
    }
    return -1;
}
/*
 * find_user_by_fd ( user **u , int fd )
 * 
 * user **u : store the result
 * int fd : file discriptor
 *
 * find a user in the user list with given fd,
 * and store the user in u.
 *
 * Returns the index of user in user list, -1 if not find 
 */
int find_user_by_fd(user **u, int fd){
    int i;
    user *tmp_usr;

    for(i = 0; i < FD_SETSIZE; i++){
        tmp_usr = user_list[i];
        if(tmp_usr && tmp_usr -> fd == fd){
            *u = tmp_usr;
            return i;
        }
    }
    return -1;
}

/*
 * find_user_by_nick ( user **u , const char *nickname )
 * 
 * user **u : store the result
 * char *nickname : target nickname
 *
 * find a user in the user list with given nickname,
 * and store the user in u.
 *
 * Returns the index of user in user list, -1 if not find 
 */
int find_user_by_nick(user **u, char *nickname){
    int i;
    user *tmp_usr;

    for(i = 0; i < FD_SETSIZE; i++){
        tmp_usr = user_list[i];
        if(tmp_usr && !strcmp(tmp_usr->nickname, nickname)){
            *u = tmp_usr;
            return i;
        }
    }
    return -1;
}

/*******************
 * End find methods
 *******************/

/*******************
 * Command handlers
 *******************/

/*
 * handle_nick( int fd, char *nickname )
 *
 * int fd : the file discriptor of the user who need change its nickname
 * char *nickname: the new nickname
 *
 * Give the user a nickname or change the previous one.
 * If the nickname is longer than the MAX_NAME_LEN or is already used by other,
 * it will report an error
 */
void handle_nick(int fd, char *nickname){
    user *u = NULL, *nick_user;

    /* Check the valid of the new nickname */
    if(sizeof(nickname) > MAX_NAME_LEN)
        return reply(fd, "NICK: %s is too long to be a nickname\r\n", nickname);

    if(find_user_by_fd(&u, fd) < 0) /* find the target user */
        app_error("Nick: No such user");

    /* Remove the '\n' or '\r\n' */
    get_msg(nickname, nickname);

    /* No duplicate user name */
    if(find_user_by_nick(&nick_user, nickname) >= 0 && (nick_user -> fd != fd)){
        return reply(fd, "NICKNAMEINUSE\r\n");
    }

    strcpy(u->nickname, nickname);

    /* If the username is already exist, then reply to the fd */
    if(strcmp(u -> username, ANONYMOUS)){
        reply(fd, ":%s 375 %s :- %s Message of the day - \r\n", u -> hostname, nickname, u -> hostname);
        reply(fd, ":%s 372 %s :- Register\r\n", u-> hostname, nickname);
        reply(fd, ":%s 376 %s :End of /MOTD command\r\n", u -> hostname, nickname);
    }
}

/*
 * void handle_user( int fd, char *username, char *hostname, char *realname )
 *
 * int fd : the file discriptor of the user who need specified it name
 *
 * Specify the username, hostname, and real name of a user.
 */
void handle_user(int fd, char *username, char *hostname, char *realname){
    user *u = NULL;

    /* Check the valid of the new name(s) */
    if(sizeof(username) > MAX_NAME_LEN)
        return reply(fd, "USER: %s is too long to be a username\r\n", username);
    if(sizeof(hostname) > MAX_NAME_LEN)
        return reply(fd, "USER: %s is too long to be a hostname\r\n", hostname);
    if(sizeof(username) > MAX_NAME_LEN)
        return reply(fd, "USER: %s is too long to be a realname\r\n", realname);

    if(find_user_by_fd(&u, fd) < 0) /* find the target user */
        app_error("USER: No such user");

    strcpy(u -> username, username);
    strcpy(u -> hostname, hostname);
    strcpy(u -> realname, realname);

    /* Without the rest code, it can pass the ruby script but not the python script */
    /* If the nickname is already exist, then reply to the fd */
    if(strcmp(u -> nickname, ANONYMOUS)){
        reply(fd, ":%s 375 %s :- %s Message of the day - \r\n", u -> hostname, u -> nickname, u -> hostname);
        reply(fd, ":%s 372 %s :- Register\r\n", u-> hostname, u -> nickname);
        reply(fd, ":%s 376 %s :End of /MOTD command\r\n", u -> hostname, u -> nickname);
    }
}

/*
 * handle_quit ( int fd )
 *
 * int fd : the file discriptor of the user who need join a channel
 *
 * End the client session. The server should announce the client’s departure to all
 * other users sharing the channel with the departing client.
 */
void handle_quit(int fd){
    int id;
    user *u;

    handle_part(fd);

    id = find_user_by_fd(&u, fd);

    Close(fd); //line:conc:echoservers:closeconnfd
    FD_CLR(fd, &p->read_set); //line:conc:echoservers:beginremove
    p->clientfd[id] = -1;          //line:conc:echoservers:endremove
    /* Free the memory of the user */
    Free(user_list[id]);
    user_list[id] = NULL;
}

/*
 * handle_join ( int fd, char *channelname )
 *
 * int fd : the file discriptor of the user who need join a channel
 * char *channelname: the target channel's name
 *
 * Start listening to a specific channel. Your server should restrict a client to be a
 * member of at most one channel. Joining a new channel should implicitly cause the client to leave
 * the current channel.
 */
void handle_join(int fd, char *channelname){
    user *u = NULL, *mate;
    channel *c;
    int i, m_id;
    char name[MAX_NAME_LEN];

    /* Check the valid of the new nickname */
    if(sizeof(channelname) > MAX_NAME_LEN)
        return reply(fd, "JOIN: %s is too long to be a channelname\r\n", channelname);

    if(find_user_by_fd(&u, fd) < 0) /* find the target user */
        app_error("JOIN: No such user");

    //quit_channel(u);

    /* Remove the '\n' or '\r\n' */
    if(get_msg(channelname, name) < 0)
        strcpy(name, channelname);

    if(find_channel_by_name(&c, name) < 0){ /* No such channel */
        c = (channel *)Malloc(sizeof(channel)); /* Create a new channel */
        for(i = 0; i < FD_SETSIZE; i++){
            if( channel_list[i] == NULL ){ /* Find an empty slot */
                /* Initiliaze the channel */
                channel_list[i] = c;
                strcpy(c -> channelname, name);
                c -> index = i;
                for( i = 0 ; i < FD_SETSIZE; i++)
                    c -> follower[i] = -1;
                break;
            }
        }
    }

    if(u -> channel >= 0)
        handle_part(fd);

    /* Add the follower */
    for(i = 0; i < FD_SETSIZE; i++){
        if(c -> follower[i] < 0){ /* Find an empty slot */
            c -> follower[i] = u -> index;
            u -> channel = c -> index;
            reply(fd, ":%s JOIN %s\r\n", u -> nickname, c -> channelname);
            break;
        }
    }

    /* List the user and declare to them */
    reply(fd, ":JOIN 353 %s = %s : ", u -> nickname, c -> channelname);
    for(i = 0; i < FD_SETSIZE; i++){
        if((m_id = c -> follower[i]) >= 0 ){
            mate = user_list[m_id];
            reply(fd, "%s ", mate -> nickname);
            if(m_id != u -> index)
                reply(mate->fd, ":%s JOIN %s\r\n", u -> nickname, c -> channelname);
        }
    }
    reply(fd, "\r\n");
    reply(fd, ":JOIN 366 %s %s :End of /NAMES list\r\n", u -> nickname, c -> channelname);
}

/*
 * handle_who ( int fd, char *channelname )
 *
 * int fd : the file discriptor of the user who need join a channel
 * char *channelname: the target channel's name
 *
 * Query information about channels. It should do an exact match on the channel
 * name and return the users on that channel.
 */
void handle_who(int fd, char *channelname){
    channel *c;
    user *mate, *u = NULL;
    int i, m_id;
    char name[MAX_NAME_LEN];

    /* Remove the '\n' or '\r\n' */
    if(get_msg(channelname, name) < 0)
        strcpy(name, channelname);

    if(find_user_by_fd(&u, fd) < 0) /* find the target user */
        app_error("WHO: No such user");

    if(find_channel_by_name(&c, name) < 0) /* Find the target channel */
        return reply(fd, "WHO: No such channel\r\n");


    /*
    char hostname[MAX_NAME_LEN];
    char realname[MAX_NAME_LEN];
    char username[MAX_NAME_LEN];
    char nickname[MAX_NAME_LEN];
    */
    reply(fd, ":WHO 352 %s %s",u -> nickname, name);
    for(i = 0; i < FD_SETSIZE; i++){
        if((m_id = c -> follower[i]) >= 0 ){ /* There is a follower */
            mate = user_list[m_id];
            reply(fd, " %s %s %s %s",  
                mate -> username,
                mate -> realname,
                mate -> hostname,
                mate -> nickname);
        }
    }
    reply(fd," H :0 The MOTD\r\n");

    reply(fd, ":WHO 315 %s %s :End of /WHO list\r\n", u -> nickname, c -> channelname);
}

/*
 * handle_list ( int fd )
 *
 * int fd : the file discriptor of the user who need join a channel
 *
 * List all existing channels on the local server only. Your server should ignore parameters 
 * and list all channels and the number of users on the local server in each channel.
 */
void handle_list(int fd){
    channel *c;
    user *u = NULL;
    int i, j, cnt;

    if(find_user_by_fd(&u, fd) < 0) /* find the target user */
        app_error("LIST: No such user");

    reply(fd, ":LIST 321 %s Channel :Users Name\r\n", u -> nickname);
    for(i = 0; i < FD_SETSIZE; i++){
        if((c = channel_list[i]) != NULL){ /* There is a channel at this slot */
            for(j = 0, cnt = 0; j < FD_SETSIZE; j++){
                if(c -> follower[j] >= 0)
                    cnt++;
            }
            /* Print channel name and  count */
            reply(fd, ":LIST 322 %s %s %d",u->nickname, c->channelname, cnt);
        }
    }
    reply(fd, "\r\n");
    reply(fd, ":LIST 323 %s :End of /LIST\r\n", u -> nickname);
}

/*
 * handle_privmsg ( int fd, char *to_nick, char *msg )
 *
 * int fd : the file discriptor of the user who need join a channel
 * char *to_nick: the target user's nickname
 * char *msg: the message need to send
 *
 * Send messages to users. The target can be either a nickname or a channel. If the target is a 
 * channel, the message will be broadcast to every user on the specified channel, except the
 * message originator. If the target is a nickname, the message will be sent only to that user. 
 */
void handle_privmsg(int fd, char *to_nick, char *msg){
    user *to, *from = NULL;
    char list[MAX_MSG_TOKENS][MAX_MSG_LEN+1];
    int i, cnt,j;
    channel *c;

    /* Remove the \r\n or \n */
    get_msg(msg, msg);

    if(find_user_by_fd(&from, fd) < 0) /* Find the callee */
        app_error("PRIVMSG: No such user");

    /* Split the message by ',' */
    cnt = tokenize((const char *)to_nick, list, ',');

    /* Scan the list */
    for(i = 0; i < cnt; i++){
        get_msg(list[i], list[i]);
        if(find_channel_by_name(&c, list[i]) >= 0){ /* Find a channel */
            for(j = 0; j < FD_SETSIZE; j++){
                if(c -> follower[j] >= 0){ /* send message to each follower of the channel */
                    to = user_list[c -> follower[j]];
                    //reply(from -> fd, ":%s PRIVMSG %s :%s\r\n", from -> nickname, c -> channelname, msg);
                    reply(to->fd, ":%s PRIVMSG %s :%s\r\n", from -> nickname, c -> channelname, msg);
                }
            }
        }else if(find_user_by_nick(&to, list[i]) >= 0){ /* Find a user */
            reply(from -> fd, ":%s PRIVMSG %s :%s\r\n", from -> nickname, to ->nickname, msg);
            reply(to->fd, ":%s PRIVMSG %s :%s\r\n", from -> nickname, to -> nickname, msg);
        }else{ /* Neither a channel nor a user */
            reply(fd, "PRIVMSG: %s not found\r\n", list[i]);
        }
    }
}

/*
 * handle_part ( int fd )
 *
 * int fd : the file discriptor of the user who need join a channel
 *
 * Depart a specific channel. Though a user may only be in one channel at a time,
 * PART should still handle multiple arguments. If no such channel exists or it exists but the user is
 * not currently in that channel, send the appropriate error message.
 */
void handle_part(int fd){
    channel *c;
    user *u = NULL, *mate;
    int i, c_id;

    if(find_user_by_fd(&u, fd) < 0) /* find the target user */
        app_error("LIST: No such user");

    if((c_id = u -> channel) < 0) /* The user is not in a channel */
        return reply(fd, "PART: You have not followed a channel\r\n");

    u -> channel = -1;

    c = channel_list[c_id]; /* Set the channel */
    for(i = 0; i < FD_SETSIZE; i++){
        if(c -> follower[i] >= 0){
            if(c -> follower[i] == u -> index){ /* Echo to self */
                reply(fd, ":%s!%s@%s QUIT :\r\n", u->nickname, u->nickname, c->channelname);
                c -> follower[i] = -1;
            }else{ /* Echo to others */
                mate = user_list[c -> follower[i]];
                reply(mate->fd, ":%s!%s@%s QUIT :\r\n", u->nickname, mate->nickname, c->channelname);
            }
        }
    }
}

/***********************
 * End command handlers
 ***********************/

/*
 * void init_node( int argc, char *argv[] )
 *
 * Takes care of initializing a node for an IRC server
 * from the given command line arguments
 */
void init_node( int argc, char *argv[] )
{
    int i;

    if( argc < 3 )
    {
        printf( "%s <nodeID> <config file>\n", argv[0] );
        exit( 0 );
    }

    /* Parse nodeID */
    curr_nodeID = atol( argv[1] );

    /* Store  */
    rt_parse_config_file(argv[0], &curr_node_config_file, argv[2] );

    /* Get config file for this node */
    for( i = 0; i < curr_node_config_file.size; ++i )
        if( curr_node_config_file.entries[i].nodeID == curr_nodeID )
             curr_node_config_entry = &curr_node_config_file.entries[i];

    /* Check to see if nodeID is valid */
    if( !curr_node_config_entry )
    {
        printf( "Invalid NodeID\n" );
        exit(1);
    }
}

/*
 * size_t get_msg( char *buf, char *msg )
 *
 * char *buf : the buffer containing the text to be parsed
 * char *msg : a user malloc'ed buffer to which get_msg will copy the message
 *
 * Copies all the characters from buf[0] up to and including the first instance
 * of the IRC endline characters "\r\n" into msg.  msg should be at least as
 * large as buf to prevent overflow.
 *
 * Returns the size of the message copied to msg.
 */
size_t get_msg(char *buf, char *msg)
{
    char *end;
    int  len;

    /* Find end of message */
    end = strstr(buf, "\r\n");

    if( end )
    {
        len = end - buf + 2;
    }
    else
    {
        /* Could not find \r\n, try searching only for \n */
        end = strstr(buf, "\n");
        if( end )
            len = end - buf + 1;
        else
            return -1;
    }

    /* found a complete message */
    memcpy(msg, buf, len);
    // buf[0] = '\r';
    // buf[1] = '\n';
    msg[end-buf] = '\0';

    return len; 
}

/*
 * int tokenize( char const *in_buf, char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN+1], char delim )
 *
 * A strtok() variant.  If in_buf is a space-separated list of words,
 * then on return tokens[X] will contain the Xth word in in_buf.
 *
 * Note: You might want to look at the first word in tokens to
 * determine what action to take next.
 *
 * Returns the number of tokens parsed.
 */
int tokenize(char const *in_buf, char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN+1], char delim) {
    int i = 0;
    const char *current = in_buf;
    int done = 0;

    /* Possible Bug: handling of too many args */
    while (!done && (i < MAX_MSG_TOKENS)) {
        char *next = strchr(current, delim);

        if (next) {
            memcpy(tokens[i], current, next-current);
            tokens[i][next - current] = '\0';
            current = next + 1; /* move over the space */
            ++i;

            /* trailing token */
            if (*current == ':') {
                ++current;
                strcpy(tokens[i], current);
                ++i;
                done = 1;
            }
        } else {
            strcpy(tokens[i], current);
            ++i;
            done = 1;
        }
    }

    return i;
}
