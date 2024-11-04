#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>

int const minuteSteps = 30;
int const timeShiftHours = -2;

// Function to check if a year is a leap year
int isLeapYear(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

// Function to get the number of days in a given month of a specific year
int daysInMonth(int year, int month) {
    int daysPerMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return daysPerMonth[month - 1];
}

// Function to convert a date to the total minutes since the beginning of the year
long int convertToMinutes(int year, int month, int day, int hour, int minute) {
    long int totalMinutes = 0;

    // Add minutes for each full year
    for (int y = 0; y < year; y++) {
        totalMinutes += 365 * 24 * 60;
        if (isLeapYear(y)) {
            totalMinutes += 24 * 60; // Add a day in leap years
        }
    }

    // Add minutes for each month in the current year
    for (int m = 1; m < month; m++) {
        totalMinutes += daysInMonth(year, m) * 24 * 60;
    }

    // Add minutes for each day, hour, and minute in the current month
    totalMinutes += (day - 1) * 24 * 60;
    totalMinutes += hour * 60;
    totalMinutes += minute;

    return totalMinutes;
}


int main() {
    FILE *file;
    char line[256];
    int aYear, aMonth, aDay;
    int aStartHour, aStartMinute, aLengthHour, aLengthMinute;
    char appointmentName[100];

    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        perror("ioctl");
        return 1;
    }

    // Adjust rows to leave space for the command prompt
    w.ws_row = w.ws_row - 1;

    // Get the current time
    time_t current_time = time(NULL)+timeShiftHours*3600;
    struct tm *local_time = localtime(&current_time);

    // Extract date and time components
    int day = local_time->tm_mday;
    int month = local_time->tm_mon + 1;
    int year = local_time->tm_year + 1900;
    int hour = local_time->tm_hour;
    int minute = local_time->tm_min;
    minute -= minute % minuteSteps;

    // Create and initialize the 2D char array with the terminal size
    char terminal_array[w.ws_row][w.ws_col];

    // Fill the array with ' '
    for (int i = 0; i < w.ws_row; i++) {
        for (int j = 0; j < w.ws_col; j++) {
            terminal_array[i][j] = ' ';
        }
    }

    char time_string[20];
    int displayMinute = minute;
    int displayHour = hour;
    for (int k = 0; k < w.ws_row; k++) {
        displayMinute += minuteSteps;
        if (displayMinute == 60) { displayMinute = 0; displayHour += 1; } 
        if (displayHour == 24) { displayHour = 0; }
        snprintf(time_string, sizeof(time_string), "%02d:%02d", displayHour, displayMinute);
        for (int i = 0; i < w.ws_col && time_string[i] != '\0'; i++) {
            terminal_array[k][i] = time_string[i];
        }
    }

    // Open the file in read mode
    file = fopen("termine", "r");
    if (file == NULL) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    int aInMinutes;
    long int appointmentStartMinutes;
    long int minutesToStart;
    long int minutesToEnd;
    long int appointmentLength;
    int x,y,nameLength;
    // Calculate the total minutes for both times
    long int currentMinutes = convertToMinutes(year, month, day, hour, minute);
    bool foundValidX;
    int overlapping;
    bool alreadyBegun;

    // Read each line in the file
    while (fgets(line, sizeof(line), file) != NULL) {
        // Parse the line with sscanf
        int matched = sscanf(line, "%d-%d-%d %d:%d + %d.%d\t%99[^\n]",
                             &aYear, &aMonth, &aDay,
                             &aStartHour, &aStartMinute,
                             &aLengthHour, &aLengthMinute,
                             appointmentName);

        if (matched == 8) {

	    appointmentStartMinutes = convertToMinutes(aYear, aMonth, aDay, aStartHour, aStartMinute);
	    minutesToStart = appointmentStartMinutes - currentMinutes;
	    appointmentLength = aLengthHour*60+aLengthMinute;
	    if (appointmentStartMinutes>currentMinutes+minuteSteps*w.ws_row || appointmentStartMinutes + appointmentLength > currentMinutes) {
	    minutesToEnd = appointmentStartMinutes+appointmentLength - currentMinutes;
	    if (minutesToEnd>w.ws_row*minuteSteps) {minutesToEnd=(w.ws_row)*minuteSteps;}
	    if (minutesToStart<0) {minutesToStart=0;alreadyBegun=true;} else {alreadyBegun=false;}

	    y=minutesToStart/minuteSteps;

// find x
x = 7;
foundValidX = false;
while (!foundValidX) {
    appointmentLength = aLengthHour*60+aLengthMinute;
    overlapping = 0;

    // Ensure we don't go out of bounds for the y coordinate
    if (y < w.ws_row) {
        for (int offset = 0; offset <= appointmentLength; offset += minuteSteps) {
            if (y + offset / minuteSteps < w.ws_row) { // Check bounds
                if (terminal_array[y + offset / minuteSteps][x] != ' ') {
                    overlapping++;
                }
            }
        }
    } else {
        // Invalid y index, break to avoid segmentation fault
        break;
    }

    if (overlapping == 0) {
        foundValidX = true;
    } else {
        x += 2;
        if (x >= w.ws_col) { // If we exceed the width of the terminal, break
            fprintf(stderr, "No valid x found for appointment %s. Terminating search.\n", appointmentName);
            break;
        }
    }
}

		//boxfill
		nameLength = 0;
		while (appointmentName[nameLength] != '\0') {
		    nameLength++;
		}
		appointmentLength = aLengthHour*60+aLengthMinute;
    		for (int i = 1; i < appointmentLength / minuteSteps; i++) {
    		    if (y + i < w.ws_row) {
    		        for (int j = 1; j < nameLength+3; j++) {
                		terminal_array[y + i][x + j] = '\''; // Fill the box with apostrophes
            		}
    		    }
    		}
	    // print appointment name
	    if (y<w.ws_row) { 
		nameLength = 0;
	    	while (nameLength < w.ws_col+1 - y && appointmentName[nameLength] != '\0') {
		    terminal_array[y][x + 2 + nameLength] = appointmentName[nameLength];
		    nameLength++;
		}
        	snprintf(time_string, sizeof(time_string), "%02d:%02d", aStartHour, aStartMinute);
       		for (int i = 0; i < w.ws_col && time_string[i] != '\0'; i++) {
        	    terminal_array[y+1][x + 2 + i] = time_string[i];
        	}
		nameLength+=3;
	    
	    // draw bottom line
	    appointmentLength = minutesToEnd - minutesToStart;
	    for (int i=1; nameLength>i; i++) {
		    if (y<w.ws_row) {
			    terminal_array[y+appointmentLength/minuteSteps][x+i]='-';
		    }
	    }

	    // draw left line 
	    appointmentLength = minutesToEnd - minutesToStart;
	    if (alreadyBegun==false) { terminal_array[y][x]='<'; }
	    else { terminal_array[y][x]='|'; terminal_array[y-1][x]='|'; }
	    terminal_array[y+appointmentLength/minuteSteps][x]='[';
	    while (appointmentLength>=2*minuteSteps) {
		    if (y<w.ws_row) {
			    terminal_array[y+appointmentLength/minuteSteps-1][x]='|';
		    }
		    appointmentLength-=minuteSteps;
	    }

	    // draw right line 
	    appointmentLength = minutesToEnd - minutesToStart;
	    if (alreadyBegun==false) { terminal_array[y][x+nameLength]='>'; }
	    	else { terminal_array[y][x+nameLength]='|'; terminal_array[y-1][x+nameLength]='|'; }
	    	terminal_array[y+appointmentLength/minuteSteps][x+nameLength]=']';
	 	   while (appointmentLength>=2*minuteSteps) {
			    if (y<w.ws_row) {
				    terminal_array[y+appointmentLength/minuteSteps-1][x+nameLength]='|';
			    }
			    appointmentLength-=minuteSteps;
	    	}
	    }
	




        } else {
            fprintf(stderr, "Error parsing line: %s", line);
        }
    }
    }

    // Close the file after reading all lines
    fclose(file);

    // Print the array
    for (int i = 0; i < w.ws_row; i++) {
        for (int j = 0; j < w.ws_col; j++) {
	    if (terminal_array[i][j]=='<') {printf("╓");}	
	    else if (terminal_array[i][j]=='>') {printf("╖");}	
	    else if (terminal_array[i][j]=='-') {printf("═");}	
	    else if (terminal_array[i][j]=='|') {printf("║");}	
	    else if (terminal_array[i][j]=='[') {printf("╚");}	
	    else if (terminal_array[i][j]==']') {printf("╝");}	
	    else if (terminal_array[i][j]==' ') {printf(" ");}	
	    else if (terminal_array[i][j]=='\'') {printf(" ");}	
	    else {putchar(terminal_array[i][j]);}
        }
        putchar('\n');  // Newline after each row
    }

    return 0;
}
