#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>

int const minuteSteps = 30;
int const timeShiftHours = -2;

// ANSI color codes
const char* colors[] = {
	"\x1b[0m",      // 0 
	"\x1b[1;30m",   // 1 Bold Black
	"\x1b[31m",     // 2 Red     - Deadlines / Exams
	"\x1b[32m",     // 3 Green   - Travel
	"\x1b[33m",     // 4 Yellow
	"\x1b[34m",     // 5 Blue    - UNI
	"\x1b[35m",     // 6 Magenta
	"\x1b[36m",     // 7 Cyan
	"\x1b[37m",     // 8 White
};

// leap year, days in month
int isLeapYear(int year) { return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));}
int daysInMonth(int year, int month) {
	int daysPerMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if (month == 2 && isLeapYear(year)) {return 29;}
	return daysPerMonth[month - 1];
}

// Convert date to minutes (since new year)
long int convertToMinutes(int year, int month, int day, int hour, int minute) {
	long int totalMinutes = 0;
	for (int y = 0; y < year; y++) {
		totalMinutes += 365 * 24 * 60;
		if (isLeapYear(y)) {totalMinutes += 24 * 60;}
	}
	for (int m = 1; m < month; m++) {totalMinutes += daysInMonth(year, m) * 24 * 60;}
	totalMinutes += (day - 1) * 24 * 60;
	totalMinutes += hour * 60;
	totalMinutes += minute;
	return totalMinutes;
}


int main() {
	char line[256];
	int aYear, aMonth, aDay, aStartHour, aStartMinute, aEndHour, aEndMinute, aLengthHour, aLengthMinute;
	char appointmentName[100];
	char appointmentTag[100];
	struct winsize w;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
		perror("ioctl");
		return 1;
	}

	// The command promt takes away one line 
	w.ws_row = w.ws_row - 1;
	
    // Current time
	time_t current_time = time(NULL)+timeShiftHours*3600;
	struct tm *local_time = localtime(&current_time);
	int day = local_time->tm_mday,
	    month = local_time->tm_mon + 1,
	    year = local_time->tm_year + 1900,
	    hour = local_time->tm_hour,
	    minute = local_time->tm_min;
	minute -= minute % minuteSteps;
	
	// Array to hold each character that is outputted to terminal
    char terminal_array[w.ws_row][w.ws_col];
	for (int i = 0; i < w.ws_row; i++) {
		for (int j = 0; j < w.ws_col; j++) {
			terminal_array[i][j] = ' ';
		}
	}
    // Color lookup array for each character
	int color_array[w.ws_row][w.ws_col];
	for (int i = 0; i < w.ws_row; i++) {
		for (int j = 0; j < w.ws_col; j++) {
			color_array[i][j] = 0;
		}
	}

    // Draw time scale on the right side
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
	
    // Open file
	FILE *file;
	file = fopen("Termine/termine", "r");
	if (file == NULL) {
		perror("Error opening file");
		return EXIT_FAILURE;
	}
	
	int aInMinutes, x, y, nameLength;
	long int appointmentStartMinutes,
         minutesToStart,
         minutesToEnd,
         appointmentLength;

	// Calculate the total minutes for both times
	long int currentMinutes = convertToMinutes(year, month, day, hour, minute);
	bool foundValidX;
	int overlapping;
	bool alreadyBegun;
	int setColor;

	// Read each line in the file
	while (fgets(line, sizeof(line), file) != NULL) {
		// Parse the line with sscanf
		int matched = sscanf(line, "%d-%d-%d %d:%d + %d.%d\t%99[^\t]\t%99[^\n]",
				&aYear, &aMonth, &aDay,
				&aStartHour, &aStartMinute,
				&aLengthHour, &aLengthMinute,
				appointmentName, appointmentTag);
		if (matched == 9) {
	
			appointmentStartMinutes = convertToMinutes(aYear, aMonth, aDay, aStartHour, aStartMinute);
			minutesToStart = appointmentStartMinutes - currentMinutes;
			appointmentLength = aLengthHour*60+aLengthMinute;
			if (appointmentStartMinutes>currentMinutes+minuteSteps*w.ws_row || appointmentStartMinutes + appointmentLength > currentMinutes) {
				minutesToEnd = appointmentStartMinutes+appointmentLength - currentMinutes;
				if (minutesToEnd>w.ws_row*minuteSteps) {minutesToEnd=(w.ws_row)*minuteSteps;}
				if (minutesToStart<0) {minutesToStart=0;alreadyBegun=true;} else {alreadyBegun=false;}
	
				y=minutesToStart/minuteSteps-1;
	
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
						x += 1;
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
				// set color
				if (strncmp(appointmentTag, 	"uni"	, 3) == 0) {setColor=5;}
				else if (strncmp(appointmentTag, 	"travel", 6) == 0) {setColor=3;}
				else if (strncmp(appointmentTag, 	"r"	, 1) == 0) {setColor=0;}
				else if (strncmp(appointmentTag, 	"social", 6) == 0) {setColor=6;}
				else {setColor=0;}
				for (int i = 0; i<appointmentLength/minuteSteps+2;i++) {
					if (y + i < w.ws_row) {
						for (int j = 0; j < nameLength+4; j++) {
							color_array[y + i][x + j] = setColor; // Fill the box with apostrophes
						}
					}
				}
	
				// print appointment name
				if (y<w.ws_row) { 
					nameLength = 0;
				while (nameLength < w.ws_col+1 - y && appointmentName[nameLength] != '\0') {
					terminal_array[y][x + 2 + nameLength] = appointmentName[nameLength];
					color_array[y][x + 2 + nameLength] = 1;
					nameLength++;
				}
				terminal_array[y][x+1] = '{';
				terminal_array[y][x+2+nameLength] = '}';
				nameLength+=3;
   	 
				// draw bottom line
				if (appointmentLength/minuteSteps>0) {
					appointmentLength = minutesToEnd - minutesToStart;
					for (int i=1; nameLength>i; i++) {
						if (y<w.ws_row) {
							terminal_array[y+appointmentLength/minuteSteps][x+i]='=';
						}
					}
				}
	
				// draw left line 
				appointmentLength = minutesToEnd - minutesToStart;
				terminal_array[y+appointmentLength/minuteSteps][x]='[';
				if (alreadyBegun==false) { 
					terminal_array[y][x]='<'; 
				} else { 
					terminal_array[y][x]='|'; terminal_array[y-1][x]='|'; 
				}
				while (appointmentLength>=2*minuteSteps) {
					if (y<w.ws_row) {
						terminal_array[y+appointmentLength/minuteSteps-1][x]='|';
					}
					appointmentLength-=minuteSteps;
				}
	
				// draw right line 
				appointmentLength = minutesToEnd - minutesToStart;
				terminal_array[y+appointmentLength/minuteSteps][x+nameLength]=']';
				if (alreadyBegun==false) { terminal_array[y][x+nameLength]='>'; }
				else { terminal_array[y][x+nameLength]='|'; terminal_array[y-1][x+nameLength]='|'; }
				while (appointmentLength>=2*minuteSteps) {
					if (y<w.ws_row) {
						terminal_array[y+appointmentLength/minuteSteps-1][x+nameLength]='|';
					}
					appointmentLength-=minuteSteps;
				}
	
				// print appointment time	   
				snprintf(time_string, sizeof(time_string), "%02d:%02d", aStartHour, aStartMinute);
				for (int i = 0; i < w.ws_col && time_string[i] != '\0'; i++) {
					terminal_array[y+1][x + 1 + i] = time_string[i];
				}
				aEndMinute=aStartMinute+aLengthMinute;
				aEndHour=aStartHour+aLengthHour;
				if (aEndMinute>59) {
					aEndMinute-=60;
					aEndHour+=1;
				}
				if (aEndHour>23) {
					aEndHour-=24;
				}
				snprintf(time_string, sizeof(time_string), "- %02d:%02d", aEndHour, aEndMinute);
				appointmentLength = minutesToEnd - minutesToStart;
					if (nameLength>13) {
						terminal_array[y+1][x + 6 ] = ' ';
						for (int i = 0; i < w.ws_col && time_string[i] != '\0'; i++) {
							terminal_array[y+1][x + 7 + i] = time_string[i];
						}
					} else if (appointmentLength>minuteSteps*2) {
						for (int i = 0; i < w.ws_col && time_string[i] != '\0'; i++) {
							terminal_array[y+2][x + 1 + i] = time_string[i];
						}
					}
				}
			} else {
//				fprintf(stderr, "Past event: %s", line);
			}
		} else {
			fprintf(stderr, "Error parsing line: %s", line);
		}
	}

	// Close the file after reading all lines
	fclose(file);

	// Print the array
	for (int i = 0; i < w.ws_row; i++) {
		for (int j = 0; j < w.ws_col; j++) {
			int color_code = color_array[i][j];
			if (color_code >= 0 && color_code < sizeof(colors) / sizeof(colors[0])) {
				printf("%s", colors[color_code]);
			}
			if (terminal_array[i][j]=='<') {printf("╓");}	
			else if (terminal_array[i][j]=='>') {printf("╖");}	
			else if (terminal_array[i][j]=='=') {printf("═");}	
			else if (terminal_array[i][j]=='|') {printf("║");}	
			else if (terminal_array[i][j]=='[') {printf("╚");}	
			else if (terminal_array[i][j]==']') {printf("╝");}	
			else if (terminal_array[i][j]=='\'') {printf(" ");}	
			else if (terminal_array[i][j]=='{') {printf("╴");}	
			else if (terminal_array[i][j]=='}') {printf("╶");}	
			else {putchar(terminal_array[i][j]);}
		}
		printf("\x1b[0m\n");
	}
	return 0;
}
