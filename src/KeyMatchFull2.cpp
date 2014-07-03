/* 
 *  Copyright (c) 2008-2010  Noah Snavely (snavely (at) cs.cornell.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

/* KeyMatchFull.cpp */
/* Read in keys, match, write results to a file */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "keys2a.h"

int ReadFileList(char* list_in, std::vector<std::string>& key_files) {
    FILE* fp;

    if ((fp = fopen(list_in, "r")) == NULL) {
        printf("Error opening file %s for reading.\n", list_in);
        return 1;
    }

    char buf[512], *start;
    while (fgets(buf, 512, fp)) {
        // Remove trailing new-line
        if (buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = '\0';

        // Find first non-space character
        start = buf;
        while(isspace(*start)) start++;

        // Skip empty lines
        if (strlen(start) == 0) continue;

        // Append file-name to key_files
        key_files.push_back(std::string(buf));
    }

    // Check we found input files
    if (key_files.size() == 0) {
        printf("No input files found in %s.\n", list_in);
        return 1;
    }

    return 0;
}

/* AHP */
void	usage(char *);
#define	DEFAULT_MATCHES_THRESHOLD	16
#define	DEFAULT_WINDOW_RADIUS		-1
#define	DEFAULT_RATIO			0.6
/* End AHP */

int main(int argc, char **argv) {
/* AHP modifications begin: */
    char *list_in = NULL;
    char *file_out = NULL;
    double ratio = DEFAULT_RATIO;
	char	*notconnected_in = NULL;
	int matches_threshold = DEFAULT_MATCHES_THRESHOLD;
	int window_radius = DEFAULT_WINDOW_RADIUS;
	int  num_skipped_pairs = 0, c;
	double	total_time_in_matching = 0.0;
	while( (c=getopt(argc, argv, "i:o:n:r:w:h")) != -1 ){
		switch(c){
			case 'i' : list_in = strdup(optarg); break;
			case 'o' : file_out = strdup(optarg); break;
			case 'r' : ratio = atof(optarg); break;
			case 'w' : window_radius = atoi(optarg); break;
			case 't' : matches_threshold = atoi(optarg); break;
			case 'n' : /* we have notconnected */
				   notconnected_in = strdup(optarg);
				   fprintf(stdout, "%s : not-connected images list supplied in '%s'.\n", argv[0], notconnected_in);
				   break;
			case 'h' : usage(argv[0]); exit(0);
			default  : usage(argv[0]); fprintf(stderr, "\n\n%s : option '-%c' not recognised.\n", argv[0], c); exit(1);
		}
	}

	if( list_in == NULL ){
		usage(argv[0]);
		fprintf(stderr, "\n%s : an input file (-i) is required with following format:\n\tkermit000.jpg 0 660.803059273422\ni.e. image name (full path ok) <space> 0 <space> Focal length in pixels for this particular image - each image in a new line.\n", argv[0]);
		return EXIT_FAILURE;
	}
	if( file_out == NULL ){
		usage(argv[0]);
		fprintf(stderr, "\n%s : an output file is required (-o).\n", argv[0]);
		return EXIT_FAILURE;  
	}
	std::pair<int,int>	a_notconnected_pair;
	std::vector< std::pair<int,int> > notconnected_pairs;
	int num_notconnected_pairs = 0;
	int num_total_pairs = 0;
	bool skip_this;
	if( notconnected_in != NULL ){
		FILE *fh = fopen(notconnected_in, "r");
		if (fh == NULL) {
			printf("%s : Error opening file '%s' for reading not connected list\n", argv[0], notconnected_in);
			return 1;
		}
		int a, b;
		while( fscanf(fh, "%d%d", &a, &b) == 2 ){
			notconnected_pairs.push_back(std::make_pair(a-1, b-1));
		}
		fclose(fh);

		num_notconnected_pairs = notconnected_pairs.size();
		for(int i=0;i<num_notconnected_pairs;i++){
			a_notconnected_pair = notconnected_pairs[i];
			fprintf(stdout, "%s : Images are not connected (%d of %d, indices start from 1) : %d %d\n", argv[0], i+1, num_notconnected_pairs, a_notconnected_pair.first+1, a_notconnected_pair.second+1);
		}
		fprintf(stdout, "%s : total not connected image pairs read from '%s' : %d x 2 = %d\n", argv[0], notconnected_in, num_notconnected_pairs, 2*num_notconnected_pairs);
	}
	/* end AHP */

    clock_t start = clock();

    /* Read the list of files */
    std::vector<std::string> key_files;
    if (ReadFileList(list_in, key_files) != 0) return EXIT_FAILURE;

    FILE *f;
    if ((f = fopen(file_out, "w")) == NULL) {
        printf("Could not open %s for writing.\n", file_out);
        return EXIT_FAILURE;
    }

    int num_images = (int) key_files.size();

    std::vector<unsigned char*> keys(num_images);
    std::vector<int> num_keys(num_images);

    /* Read all keys */
    for (int i = 0; i < num_images; i++) {
        keys[i] = NULL;
        num_keys[i] = ReadKeyFile(key_files[i].c_str(), &keys[i]);
	/* AHP */
	fprintf(stdout, "%s : read %d keys from file '%s' (%d of %d).\n", argv[0], num_keys[i], key_files[i].c_str(), i+1, num_images);
	/* END AHP */
    }

    clock_t end = clock();
    printf("[KeyMatchFull] Reading keys took %0.3fs\n", 
           (end - start) / ((double) CLOCKS_PER_SEC));

    for (int i = 0; i < num_images; i++) {
        if (num_keys[i] == 0)
            continue;

        printf("[KeyMatchFull] Matching to image %d\n", i);

        start = clock();

        /* Create a tree from the keys */
        ANNkd_tree *tree = CreateSearchTree(num_keys[i], keys[i]);

        /* Compute the start index */
        int start_idx = 0;
        if (window_radius > 0) 
            start_idx = std::max(i - window_radius, 0);

        for (int j = start_idx; j < i; j++) {
            if (num_keys[j] == 0)
                continue;

		/* AHP */
		num_total_pairs++;

		/* if the pair of images is in the 'notconnected_pairs' then skip */
		skip_this = 0;
		for(int k = 0; k < num_notconnected_pairs; k++){
			a_notconnected_pair = notconnected_pairs[k];
			if( ((a_notconnected_pair.first == i) || (a_notconnected_pair.second == i)) &&
			    ((a_notconnected_pair.first == j) || (a_notconnected_pair.second == j)) ){
				fprintf(stdout, "%s : skipping pair of images (%d,%d) because it is in the not-connected list.\n", argv[0], i+1, j+1);
				num_skipped_pairs++;
				skip_this = 1; break;
			}
		}
		if( skip_this == 1 ) continue;
                fprintf(stdout, "[%s] image %d : matching to image %d\n", argv[0], i+1, j+1); /* AHP */

		/* end AHP */

            /* Compute likely matches between two sets of keypoints */
            std::vector<KeypointMatch> matches = 
                MatchKeys(num_keys[j], keys[j], tree, ratio);

            int num_matches = (int) matches.size();

            if (num_matches >= matches_threshold) {
                /* Write the pair */
                fprintf(f, "%d %d\n", j, i);

                /* Write the number of matches */
                fprintf(f, "%d\n", (int) matches.size());

                for (int i = 0; i < num_matches; i++) {
                    fprintf(f, "%d %d\n", 
                            matches[i].m_idx1, matches[i].m_idx2);
                }
            }
        }

        end = clock();
        printf("[KeyMatchFull] Matching took %0.3fs\n", 
               (end - start) / ((double) CLOCKS_PER_SEC));
	/* AHP */
	total_time_in_matching += (end - start) / ((double) CLOCKS_PER_SEC);
	/* END AHP */

        fflush(stdout);

        delete tree;
    }

    /* Free keypoints */
    for (int i = 0; i < num_images; i++) {
        if (keys[i] != NULL)
            delete [] keys[i];
    }

    fclose(f);
	/* AHP */
	if( notconnected_in != NULL ){
		fprintf(stdout, "[%s] : summary:\n\ttotal number of images : %d,\n\ttotal number of unique image pairs with valid keys and num keys>0 : %d,\n\ttotal number of image pairs skipped because of not-connected images info read from file '%s' : %d,\n\ttotal savings of %.2f%%,\n\ttotal actual time spent in matching : %.2lf seconds.\n",
			argv[0],
			num_images,
			num_total_pairs,
			notconnected_in,
			num_skipped_pairs,
			100.0 * num_skipped_pairs/num_total_pairs,
			total_time_in_matching
		);
	} else {
		fprintf(stdout, "[%s] : summary:\n\ttotal number of images : %d,\n\ttotal number of unique image pairs with valid keys and num keys>0 : %d,\n\ttotal actual time spent in matching : %.2lf seconds.\n",
			argv[0],
			num_images,
			num_total_pairs,
			total_time_in_matching
		);
	}
	/* END AHP */
    return EXIT_SUCCESS;
}

void	usage(char *appname){
	fprintf(stderr, "Usage : %s -i list.txt -o outfile [-n notconnected.txt] [-r ratio] [-t matches_threshold]\n", appname);
	printf("The optional file 'notconnected.txt' contains pair of images which should not be matched. No key matching will be done on these images. The format of the file is A B, where A and B are image indices (STARTING FROM ONE - starting from 1!!!!) referring to the order of the images in the input file 'list.txt'.\n");
	printf("Original source code by Noah Snavely.\n");
	printf("Not-connected images functionality by Andreas Hadjiprocopis.\n");
}
