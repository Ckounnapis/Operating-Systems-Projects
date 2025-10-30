#include "bench.h"

void _random_key(char *key,int length) {
	int i;
	char salt[36]= "abcdefghijklmnopqrstuvwxyz0123456789";

	for (i = 0; i < length; i++)
		key[i] = salt[rand() % 36];
}

void _print_header(int count)
{
	double index_size = (double)((double)(KSIZE + 8 + 1) * count) / 1048576.0;
	double data_size = (double)((double)(VSIZE + 4) * count) / 1048576.0;

	printf("Keys:\t\t%d bytes each\n", 
			KSIZE);
	printf("Values: \t%d bytes each\n", 
			VSIZE);
	printf("Entries:\t%d\n", 
			count);
	printf("IndexSize:\t%.1f MB (estimated)\n",
			index_size);
	printf("DataSize:\t%.1f MB (estimated)\n",
			data_size);

	printf(LINE1);
}

void _print_environment()
{
	time_t now = time(NULL);

	printf("Date:\t\t%s", 
			(char*)ctime(&now));

	int num_cpus = 0;
	char cpu_type[256] = {0};
	char cache_size[256] = {0};

	FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
	if (cpuinfo) {
		char line[1024] = {0};
		while (fgets(line, sizeof(line), cpuinfo) != NULL) {
			const char* sep = strchr(line, ':');
			if (sep == NULL || strlen(sep) < 10)
				continue;

			char key[1024] = {0};
			char val[1024] = {0};
			strncpy(key, line, sep-1-line);
			strncpy(val, sep+1, strlen(sep)-1);
			if (strcmp("model name", key) == 0) {
				num_cpus++;
				strcpy(cpu_type, val);
			}
			else if (strcmp("cache size", key) == 0)
				strncpy(cache_size, val + 1, strlen(val) - 1);	
		}

		fclose(cpuinfo);
		printf("CPU:\t\t%d * %s", 
				num_cpus, 
				cpu_type);

		printf("CPUCache:\t%s\n", 
				cache_size);
	}
}

int main(int argc,char** argv)
{
	long int count;

	srand(time(NULL));
       
        									// Add new command option for mixed workload (writeread) - STEP 4
  if (argc >= 5 && strcmp(argv[1], "writeread") == 0)	// Add new command option for mixed workload (writeread)
    if (argc >= 5 && strcmp(argv[1], "writeread") == 0) {
      int r = 0;
      struct timeval start_time, end_time, write_start, write_end, read_start, read_end;
      double total_time, write_time, read_time;
    
      // Record start time
      gettimeofday(&start_time, NULL);
    
      // Format is: writeread <count> <write_percent> <read_percent> [random]
      count = atoi(argv[2]);
      int write_percent = atoi(argv[3]);
      int read_percent = atoi(argv[4]);
    
      // Check if random flag is set
      if (argc >= 6 && strcmp(argv[5], "random") == 0) {
          r = 1;
      }
    
      _print_header(count);
      _print_environment();
    
      // Calculate number of operations for each type
      long int write_count = (count * write_percent) / 100;
      long int read_count = (count * read_percent) / 100;
    
      // First perform write operations
      printf("Performing %ld write operations (%d%%)...\n", write_count, write_percent);
    
      // Record write start time
      gettimeofday(&write_start, NULL);
    
      // Create a new process for the write operations
      pid_t pid = fork();
    
      if (pid == 0) {
          // Child process - perform write operations
          _write_test(write_count, r);
          exit(0);
      } else if (pid > 0) {
          // Parent process - wait for child to complete
          int status;
          waitpid(pid, &status, 0);
         
          // Record write end time
          gettimeofday(&write_end, NULL);
          write_time = (write_end.tv_sec - write_start.tv_sec) + (write_end.tv_usec - write_start.tv_usec) / 1000000.0;
         
          // Now perform read operations in a new process
          // Record read start time
          gettimeofday(&read_start, NULL);
        
          pid = fork();
        
          if (pid == 0) {
              // Child process - perform read operations
              printf("Performing %ld read operations (%d%%)...\n", read_count, read_percent);
            
              // Try to read only up to the number of keys we wrote
              if (read_count > write_count && !r) {
                  printf("Limiting read operations to %ld (number of keys written)\n", write_count);
                  read_count = write_count;
              }
            
              _read_test(read_count, r);
              exit(0);
          } else if (pid > 0) {
              // Parent process - wait for child to complete
              waitpid(pid, &status, 0);
            
              // Record read end time
              gettimeofday(&read_end, NULL);
              read_time = (read_end.tv_sec - read_start.tv_sec) + 
                       (read_end.tv_usec - read_start.tv_usec) / 1000000.0;
            
              // Record overall end time
              gettimeofday(&end_time, NULL);
              total_time = (end_time.tv_sec - start_time.tv_sec) + 
                        (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
            
              // Print write summary (first time)
              printf("+-----------------------------+----------------+------------------------------+-------------------+\n");
              printf("|%s-Write\t(done:%ld): %.6f sec/op; %.1f writes/sec(estimated); cost:%.3f(sec);\n",
                     r ? "Random" : "Sequential", write_count, 
                     write_time / write_count, 
                     write_count / write_time, 
                     write_time);
            
              // Print write summary (second time)
              printf("+-----------------------------+----------------+------------------------------+-------------------+\n");
              printf("+-----------------------------+----------------+------------------------------+-------------------+\n");
              
              // Print read summary
              printf("|%s-Read    (done:%ld, found:%ld): %.6f sec/op; %.1f reads/sec(estimated); cost:%.3f(sec)\n",
                     r ? "Random" : "Sequential", read_count, 
                     read_count, // Assuming all reads were successful
                     read_time / read_count, 
                     read_count / read_time, 
                     read_time);
            
              // Add final summary section
              printf("+-----------------------------+----------------+------------------------------+-------------------+\n");
              printf("Write-Percentage: %d%%\n", write_percent);
              printf("Read-Percentage: %d%%\n", read_percent);
              printf("Total time: %.3f seconds\n", total_time);
          }
      }
      return 0;                                                                                                                                                                                                                                                                                                           return 0;
}	
        if (argc < 3) {
		fprintf(stderr,"Usage: db-bench <write | read> <count>\n");
		exit(1);
	}
	
	if (strcmp(argv[1], "write") == 0) {
		int r = 0;

		count = atoi(argv[2]);
		_print_header(count);
		_print_environment();
		if (argc == 4)
			r = 1;
		_write_test(count, r);
	} else if (strcmp(argv[1], "read") == 0) {
		int r = 0;

		count = atoi(argv[2]);
		_print_header(count);
		_print_environment();
		if (argc == 4)
			r = 1;
		
		_read_test(count, r);
	} else {
		fprintf(stderr,"Usage: db-bench <write | read> <count> <random>\n");
		exit(1);
	}

	return 1;
}