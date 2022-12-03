#include <stdio.h>
#include<math.h>
#include <string.h>

int ENABLE_LOG = 1;
char log_str[10000];
FILE *log_file;
int LOG_SIZE = 0;

void LogInit(){
        char filename[]="/tmp/log.txt";
        log_str[0] = 0;
        log_file = ENABLE_LOG==1 ? fopen(filename,"a") : NULL;
        if(log_file==NULL){
                printf("Error opening log file\n");
//                struct stat s;
//                if(0==stat(filename, &s))
//                        sprintf(log_str, "Unable to open %s: owned by %d\n",
//                                               filename, s.st_uid);
//                else
//                        sprintf(log_str, "Unable to open %s\n", filename);
        }
}


void LogStr(char* s){
        if(LOG_SIZE < snprintf(NULL, 0, "%s %s", log_str, s)){
                if(ENABLE_LOG) fprintf(log_file ? log_file :
	                       stdout, "%s  ", log_str);
                strncpy(log_str, s, LOG_SIZE);
        }
        else
                sprintf(log_str, "%s  %s", log_str, s);
}

double SSDWR(double * e, double * o, int bins, double diff_exp){
        double sum = 0;
        for(int i=0; i < bins; ++i){
                sum += pow( o[i]-e[i], diff_exp) / e[i];
        }
        return sum;
}

int statistic_test(double chi2, double threshold){
    	return chi2 > threshold;
}

int match(double * test, double * reference, int bins, double threshold){
//        glob.bins      = bins;
//        glob.threshold = threshold;
        double diff_exp  = 2.0;
	LogInit();
        LogStr("Test params: ");
        LogStr("exp=");       // LogFloat(diff_exp);
        LogStr("threshold="); // LogFloat(threshold);
        int ret = !statistic_test(SSDWR(reference, test, bins, diff_exp), threshold);
        LogStr("result="); LogStr(ret ? "yes" : "no"); LogStr("\n");
//        LogFlush();
        return ret;
}
int main(){

    double test[] = {384,132,567,127,742};
    double reference[] = {578,485,848,237,731};
    int bins = 5;
    double threshold = 10;
    match(test, reference, bins, threshold);
// set the uid of /tmp/log.txt to a five-digit number. Tinkering with the uid of a log file causes the program to misbehave.
// call match
return 0;
}
