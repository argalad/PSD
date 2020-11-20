#include "game.h"

/** Debug server? */
#define SERVER_DEBUG 1

/** Sockets of a game used by a thread in the server */
typedef struct threadArgs{
	int socketPlayer1;
	int socketPlayer2;
} tThreadArgs;

/**
 * Function that shows an error message.
 *
 * @param msg Error message.
 */
void showError(const char *msg);

/**
 * Function that executes a turn
 * @param activePlayer Player that plays the turn
 * @param inactivePlayer Player that waits the turn
 * @param session Session of the game
*/
void turn (int activePlayer, int inactivePlayer, tSession *session, tDeck *deck);

void manage_bet (int socketPlayer, unsigned int *stack, unsigned int *bet);

void send_deck (int socketPlayer, tDeck *deck);

/**
 * Function executed by each thread.
 *
 * @param threadArgs Argument that contains the socket of the players.
 */
void *threadProcessing(void *threadArgs);
