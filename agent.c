#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datatypes.h"
#include "network.h"
#include "common.h"
#include "arguments.h"



int main(int argc, char **argv) 
{
	agent_t agent; 

	get_arguments(&agent.options, argc, argv); 
	initialize_agent(&agent); 


















	return EXIT_SUCCESS; 
}
