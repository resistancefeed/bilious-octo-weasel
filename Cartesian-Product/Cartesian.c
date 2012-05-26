
/*Joshua Beninson*/

#include <stdio.h>
#include <string.h>

int main(){
	char *cart[1000][1000];
	char input[1000];
	
	int sets;
	int i;

	printf("Please input the number of sets you wish to find the Cartesian Product for: \n");
	scanf("%i", &sets);

	/*takes in the remaining newline character left from scanf
	 * to clear it from the stdin buffer, it will be overwritten in upcoming loop.*/
	fgets(input,1000,stdin);

	printf("\nEach set can handle up to 1000 elements.");
	printf("\nPlease input elements of up to nine characters into each set.\n");
	printf("Each element should be separated by commas.\n");
	printf("Press <ENTER> to complete input for a designated set.\n");
	printf("Press <Enter> as the first element of a set for a NULL set............\n\n");

	int element;
	/*element to keep track of the element in each sets array*/
	
	/*The elements_per_set array keeps track of the amount of elements input into each set.
	 * The corresponding element of its array corresponds to the set number it is holding the
	 * value for.*/
	int elements_per_set[1000];
	
	/*The output ticker is used to keep track of the amount of times a particular element should
	 * be printed before shifting to the next element in the set.*/
	int output_ticker[1000];
	
	for(i=0;i<sets;i++){
		printf("Set %i: ",i+1);
		fgets(input,1000,stdin);
		element=0;

		/*replace the terminating newline character from fgets()
		with a NULL character*/
		if(input[strlen(input)-1]=='\n');
			input[strlen(input)-1] = '\0';
		
		/*if a null set is entered, input null into the set and skip to the next iteration of
		 * the outer loop*/	
		if(input[0]=='\0'){
			cart[i][0]=='\0';
			elements_per_set[i]=0;
			continue;			
		}

		char *result=NULL;
		char *save_ptr = NULL;

		/*parse the input stream into tokens, delimited by commas, and inserts it into the sets array.
		 * each parsed string gets inserted into it's own element of the set
		 * To explain it another way, in a two dimensional array, each set is a row and 
		 * each column contains the elements in set*/
		save_ptr = input;
		result = strtok_r(input,",",&save_ptr);
		cart[i][element]=strdup(result);
		if(result)
			elements_per_set[i]=element+1;  
		

		while((result = strtok_r(NULL,",",&save_ptr)) !=NULL){
			cart[i][++element]=strdup(result); 		
     		elements_per_set[i]=element+1;     		
		}
	}
	/*Prints: Set1 x Set2 x ... = */
	printf("\n");
	for(i=0;i<sets;i++){
		printf("Set %i ",i+1);
		if(i<(sets-1))
			printf("x ");		
	}
	printf("=\n");
		
	/*Calulates the amount of prints that must be completed before iterating to the next
	 * element in the set for each set and enters it into each sets spot in the output_ticker
	 * array*/
	int j=0;
	for(i=0;i<sets;i++){
		output_ticker[i]=1;
		for(j=(i+1);j<sets;j++){
			output_ticker[i]*=elements_per_set[j];
		}
		if(elements_per_set[i]==0){
			printf("{ }\n\n");
			return 0;
		}
	}

	/*This loop traverses each set during each print iteration and finds the appropriate element
	 * to output in each set based on the integer output of (print divided by the output ticker of
	 * the set that is being considered) and then finding the mod of that with repsect to the elements
	 * in that set in order to not traverse past the entered values in each set*/
	int print=0;
	printf("{ ");
	for(print=0;print<(output_ticker[0]*elements_per_set[0]);print++){
		printf("( ");
			for(i=0;i<sets;i++){
				element=(print/output_ticker[i])%elements_per_set[i];					
				printf("%s",cart[i][element]);
				if(i<(sets-1))
					printf(", ");				
			}
		printf(" )");
		if(print<(output_ticker[0]*elements_per_set[0])-1)
			printf(",\n");	
	}
	printf(" }\n\n");
    return 0;
}



