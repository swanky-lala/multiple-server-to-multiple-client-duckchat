#ifndef DUCKCHAT_H
#define DUCKCHAT_H

/* Path names to unix domain sockets should not be longer than this */
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

/* This tells gcc to "pack" the structure.  Normally, gcc will
 * inserting padding into a structure if it feels it is convenient.
 * When the structure is packed, gcc gaurantees that all the bytes
 * will fall exactly where specified. */
#define packed __attribute__((packed))

/* Define the length limits */
#define MAX_CONNECTIONS         10
#define HOSTNAME_MAX            100
#define MAX_MESSAGE_LEN         65536
#define USERNAME_MAX            32
#define CHANNEL_MAX             32
#define SAY_MAX                 64
#define CHANNEL_ID_MAX          HOSTNAME_MAX+6+CHANNEL_MAX+2
/* Define some types for designating request and text codes */
typedef int                     request_t;
typedef int                     text_t;

/* Define codes for request types.  These are the messages sent to the server. */
#define                         REQ_LOGIN 0
#define                         REQ_LOGOUT 1
#define                         REQ_JOIN 2
#define                         REQ_LEAVE 3
#define                         REQ_SAY 4
#define                         REQ_LIST 5
#define                         REQ_WHO 6
#define                         REQ_KEEP_ALIVE 7 /* Only needed by graduate students */

/* Used for S2S requests */
#define                         REQ_S2S_JOIN 8
#define                         REQ_S2S_LEAVE 9
#define                         REQ_S2S_SAY 10
#define                         REQ_S2S_RECV_JOIN 11
#define                         REQ_S2S_RECV_SAY 12
#define                         REQ_S2S_RENEW 13

/* Define codes for text types.  These are the messages sent to the client. */
#define TXT_SAY 0
#define TXT_LIST 1
#define TXT_WHO 2
#define TXT_ERROR 3

/* This structure is used for a generic request type, to the server. */
struct request {
        request_t req_type;
} packed;

/* Once we've looked at req_type, we then cast the pointer to one of
 * the types below to look deeper into the structure.  Each of these
 * corresponds with one of the REQ_ codes above. */

struct request_login {
        request_t req_type; /* = REQ_LOGIN */
        char req_username[USERNAME_MAX];
} packed;

struct request_logout {
        request_t req_type; /* = REQ_LOGOUT */
} packed;

struct request_join {
        request_t req_type; /* = REQ_JOIN */
        char req_channel[CHANNEL_MAX]; 
} packed;

struct request_leave {
        request_t req_type; /* = REQ_LEAVE */
        char req_channel[CHANNEL_MAX]; 
} packed;

struct request_say {
        request_t req_type; /* = REQ_SAY */
        char req_channel[CHANNEL_MAX]; 
        char req_text[SAY_MAX];
} packed;

struct request_list {
        request_t req_type; /* = REQ_LIST */
} packed;

struct request_who {
        request_t req_type; /* = REQ_WHO */
        char req_channel[CHANNEL_MAX]; 
} packed;

struct request_keep_alive {
        request_t req_type; /* = REQ_KEEP_ALIVE */
} packed;

/* Provide structures for S2S communication */

struct request_s2s_join{
        request_t req_type;
        char req_channel[CHANNEL_MAX];
} packed;

struct request_s2s_leave{
        request_t req_type;
        char req_channel[CHANNEL_MAX];
} packed;
struct request_s2s_renew{
        request_t req_type;
        char req_channel[CHANNEL_MAX];
}packed;
struct request_s2s_say{
        request_t req_type;
        char    unique_id[32];
        char    username[USERNAME_MAX];
        char    channel[CHANNEL_MAX];
        char    request_text[SAY_MAX];
} packed;

struct request_s2s_recv{
        request_t req_type;
        char    chann_name[CHANNEL_MAX];
}packed;

/* This structure is used for a generic text type, to the client. */
struct text {
        text_t txt_type;
} packed;

/* Once we've looked at txt_type, we then cast the pointer to one of
 * the types below to look deeper into the structure.  Each of these
 * corresponds with one of the TXT_ codes above. */

struct text_say {
        text_t txt_type; /* = TXT_SAY */
        char txt_channel[CHANNEL_MAX];
        char txt_username[USERNAME_MAX];
        char txt_text[SAY_MAX];
} packed;

/* This is a substructure used by struct text_list. */
struct channel_info {
        char ch_channel[CHANNEL_MAX];
} packed;

struct text_list {
        text_t txt_type; /* = TXT_LIST */
        int txt_nchannels;
        struct channel_info txt_channels[0]; // May actually be more than 0
} packed;

/* This is a substructure used by text_who. */
struct user_info {
        char us_username[USERNAME_MAX];
};

struct text_who {
        text_t txt_type; /* = TXT_WHO */
        int txt_nusernames;
        char txt_channel[CHANNEL_MAX]; // The channel requested
        struct user_info txt_users[0]; // May actually be more than 0
} packed;

struct text_error {
        text_t txt_type; /* = TXT_ERROR */
        char txt_error[SAY_MAX]; // Error message
};

/* Create Singly Linked List that will hold Unique Ids*/
struct unique_list{
        char unique_id[32];
        struct unique_list *next;
};
/*
 * Used for S2S communication and broadcasting
 * Forward list holds informations about servers that are in route table
 * When forwarding packets, use forward list...
*/
struct forward_list{
        //addres of server 
        char addr[HOSTNAME_MAX];
        //server port
        int port;
        //last minute joined
        int last_min=-1;
        //if active then 1, else 0
        int active=1;
        //if 1- you have authority they are sending JOIN to you
        //if 0- you send to authority 1
        int authority;

        //pointer to next server
        struct forward_list *next=NULL;
};

/*
 * Create routing table for each channel
 * When is server subscribed to server with chann_id it broadcast message
 * though his routing table
 * Example of chann_id is [ 127.0.0.1:22222:Common ]
*/

struct route_table{
        char chann_name[CHANNEL_MAX];

        int mystate=1;
        struct forward_list *fw=NULL;

        struct route_table *next_rt=NULL;
};

/*
 * In this DS store all adjacent servers
 * They are provided via command line
 * They will be used to form routing table for specific server and channel
*/
struct adjacent_servers{
        char addr[HOSTNAME_MAX];
        int port;
        struct adjacent_servers *next;
};
#endif
