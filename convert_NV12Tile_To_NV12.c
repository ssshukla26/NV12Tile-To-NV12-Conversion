#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

//Macro to define output format, either "nv12" or "yuv420p", no other options available
#define OUTPUT_FORMAT "nv12"

//Macro to round up a number "num" to the given number "X"
#define ROUND_UP_X(num, x) (((num)+(x-1))&~(x-1))

//Size of a single tile
#define TILE_SIZE (64*32)

//Data type of a single pixel
#define pixel uint8_t

/* Structure to hold required parameters for a nv12 tile buffer.
 * wTiles               - No of tiles required horizontally for both Luma and Chroma Planes.
 * hTiles               - No of tiles required vertically for Luma Plane
 * hTiles_UV            - No of tiles required vertically for Chroma Plane
 * frame_size_src_Y     - Size of Luma Plane
 * frame_size_src_UV    - Size of Chroma Plane
 * */
struct nv12tile_params {
    int width;
    int height;
    int ex_width;
    int ex_height;
    int wTiles;
    int hTiles;
    int hTiles_UV;
    int frame_size_src_Y;
    int frame_size_src_UV;
    int frame_size_src;
    int frame_size_dst;
    int max_rows_dst;
    pixel *src_buf;
    pixel *dst_buf;
    int frameCount;
} nv12tile_params;

/*
 * This function returns no of tiles required
 * for given width.
 * */
int calc_wTiles(int width)
{
    return (ROUND_UP_X(width, 128)/64);
}

/*
 * This function returns no of tiles required
 * for given height.
 * */
int calc_hTiles(int height)
{
    return (ROUND_UP_X(height, 32)/32);
}

/*
 * This function will returns padding required
 * for given tiles (wTiles x hTiles).
 * */
int calc_boundary_padding(int wTiles, int hTiles)
{
    float data_size = ((float)((float)(((float)wTiles * (float)hTiles * (float)TILE_SIZE)/(float)(4 * TILE_SIZE))));

    data_size = data_size - (float)((int)data_size);

    return (int)(data_size * (float)(4 * TILE_SIZE));
}

/*
 * This function will returns size required
 * for a plane of given tiles (wTiles x hTiles).
 * */
int calc_plane_size(int wTiles, int hTiles)
{
    return ((TILE_SIZE * wTiles * hTiles) + calc_boundary_padding(wTiles, hTiles));
}

/*
 * This function will initialize parameter needed
 * for a nv12 tile buffer of given width and height.
 * */
struct nv12tile_params *nv12tile_params_init(int _width, int _height)
{
    struct nv12tile_params *params = NULL;

    if(NULL != (params = calloc(1, sizeof(struct nv12tile_params)))) {

        //width
        params->width = _width;

        //height
        params->height = _height;

        //Extrapolate width
        params->ex_width = ROUND_UP_X(params->width, 128);

        //Extrapolate height
        params->ex_height = ROUND_UP_X(params->height, 32);

        //Minimum no of columns required
        params->wTiles = calc_wTiles(params->width);

        //Minimum no of rows required for Y plane
        params->hTiles = calc_hTiles(params->height);

        //Minimum no of rows required for UV plane
        params->hTiles_UV = calc_hTiles(params->height/2);

        //Size of Luma/Y Plane
        params->frame_size_src_Y = calc_plane_size(params->wTiles, params->hTiles);

        //Size of Chroma/UV Plane
        params->frame_size_src_UV = calc_plane_size(params->wTiles, params->hTiles_UV);

        //Size of a single frame in source, source is in nv12 tile format
        params->frame_size_src = params->frame_size_src_Y + params->frame_size_src_UV;

        //Size of a single frame in destination, destination is in nv12 format
        params->frame_size_dst = (params->width * params->height * 3)/2;

        //Max no. of rows in destination buffer
        params->max_rows_dst = ((params->height * 3)/2);


        //Buffer to accumulate a frame from the source
        if(NULL == (params->src_buf = (pixel *) calloc(1, params->frame_size_src * sizeof(pixel)))) {
            printf("Error: allocating source buffer\n");
            return NULL;
        }

        //Buffer to accumulate a frame for the destination
        if(NULL == (params->dst_buf = (pixel *) calloc(1, params->frame_size_src * sizeof(pixel)))) {
            printf("Error: allocation destination buffer\n");
            return NULL;
        }

        //Variable to count no of frames converted from tile to linear format i.e. nv12 tile to nv12 format
        params->frameCount = 1;
    }

    return params;
}

/*
 * This function will de-initialize nv12tile parameters.
 * */
void nv12tile_params_deinit(struct nv12tile_params *params)
{
    //Free params
    if(params) {

        //Free source buffer
        if(params->src_buf) {

            free(params->src_buf);
            params->src_buf = NULL;
        }

        //Free destination buffer
        if(params->dst_buf) {

            free(params->dst_buf);
            params->dst_buf = NULL;
        }

        free(params);
    }
}

/*
 * This functions copy a single tile from source to destination.
 * */
void copyTile(pixel *dst, pixel **src, int wTiles)
{
    //Loop variable
    int loop = 0;

    //Run loop for all rows in a tile i.e. from row 0 to row 31
    for(loop=0;loop<32;loop++) {

        //Copy a single row from source to destination
        memcpy(dst, *src, 64 * sizeof(pixel));

        //Move source pointer to next row
        *src = *src + 64;

        //Move destination pointer to next row
        dst = dst + (wTiles * 64);
    }
}

/*
 * This function is use to convert data from nv12 tile format
 * to nv12 format.
 *
 * dst_head :- destination pointer to store data in nv12 format
 * src_head :- source pointer, pointing to a data in nv12 tile format
 * wTiles :- No of horizontal tiles
 * hTiles :- No of vertical tiles
 * */
void NV12TileToNV12(pixel *dst_head, pixel *src_head, int wTiles, int hTiles)
{
    //Loop variable
    int loop = 0;

    //For Z Flip Z pattern
    int Z = 0;

    //Assign source to source head pointer
    pixel *src = src_head;

    //Assign destination to destination head pointer
    pixel *dst = dst_head;

    //For a proper Z Flip Z pattern two consecutive row are needed to be processed
    for(loop = 0; loop < (hTiles%2 == 0 ? hTiles : hTiles-1); loop=loop+2) {

        //Put src pointer at start of current row
        src = src_head;

        //Copy Z and flip Z pattern to destination
        for(Z = 0; Z < (wTiles/2); Z++) {

            //Put dst pointer at start of current row
            dst = dst_head;

            if(Z%2 == 0) {

                //For Z pattern

                //Tile Nos: 0-8-16-...
                dst = dst + (Z * 2 * 64);
                copyTile(dst, &src, wTiles);

                //Tile Nos: 1-9-17-...
                dst = dst + 64;
                copyTile(dst, &src, wTiles);

                //Tile Nos: 2-10-18-...
                dst = dst_head + (wTiles * TILE_SIZE);
                dst = dst + (Z * 2 * 64);
                copyTile(dst, &src, wTiles);

                //Tile Nos: 3-11-19-...
                dst = dst + 64;
                copyTile(dst, &src, wTiles);
            }
            else {

                //For Flip Z pattern

                //Tile Nos: 4-12-...
                dst = dst_head + (wTiles * TILE_SIZE);
                dst = dst + (Z * 2 * 64);
                copyTile(dst, &src, wTiles);

                //Tile Nos: 5-13...
                dst = dst + 64;
                copyTile(dst, &src, wTiles);

                //Tile Nos: 6-14-...
                dst = dst_head + (Z * 2 * 64);
                copyTile(dst, &src, wTiles);

                //Tile Nos: 7-15-...
                dst = dst + 64;
                copyTile(dst, &src, wTiles);
            }
        }

        //Move source pointer by two chunk rows
        src_head = src_head + (2 * TILE_SIZE * wTiles);

        //Move destination pointer by two chunk rows
        dst_head  = dst_head + (2 * wTiles * TILE_SIZE);
    }

    //For linear pattern in the last remaining row
    if(hTiles%2) {

        //Put src pointer at start of current row
        src = src_head;

        //Loop through linear tiles
        for(loop = 0; loop < wTiles ; loop++) {

            dst = dst_head + (loop * 64);
            copyTile(dst, &src, wTiles);
        }
    }
}


/*
 * This function is use to convert extrapolated nv12 data
 * to actual nv12 data.
 *
 * params :- pointer to nv12tile_params structure
 * */
void ConvertToActualNV12(struct nv12tile_params *params)
{
    //Check if width is not equal to extrapolated width
    //then perform the conversion
    if(params->width != params->ex_width) {

        //Index of current row
        int index = 0;

        //Convert extrapolated NV12 data to actual NV12 data
        while(index < params->max_rows_dst) {

            //Rectify each strides in destination buffer containing extrapolated data,
            //so that each strides will contain actual data.
            memcpy(params->dst_buf + (index * params->width),
                    params->dst_buf + (index * params->ex_width),
                    (params->width * sizeof (pixel)));

            index++;
        }
    }
}

/*
 * This function is use to convert an entire frame from nv12 tile format
 * to nv12 format.
 *
 * params :- pointer to nv12tile_params structure
 * */
void ConvertNV12TileToNV12Frame(struct nv12tile_params *params)
{
    //Convert Y plane from nv12 tile to nv12 format
    NV12TileToNV12(params->dst_buf,
            params->src_buf,
            params->wTiles,
            params->hTiles);

    //Convert UV plane from nv12 tile to nv12 format
    NV12TileToNV12(params->dst_buf + (params->ex_width * params->height),
            params->src_buf + params->frame_size_src_Y,
            params->wTiles,
            params->hTiles_UV);

    //If extrapolation is performed, convert extrapolated data to actual data
    ConvertToActualNV12(params);
}

/*
 * This function is use to convert NV12 data to
 * yuv420 planner data.
 *
 * dst_buf  :- Pointer to a destination buffer
 * src_buf  :- Pointer to a source buffer (in NV12 format)
 * width    :- Width of source buffer
 * height   :- Height of source buffer
 *
 * NOTE : Width and Height of source buffer is same as
 * Width and height of destination buffer. That's will be
 * the case always.
 * */
void NV12toYUV420Planner(pixel *dst_buf, pixel *src_buf, int width, int height)
{
    //Length of a luma plane
    int luma_plane_len = (width * height);

    //Length of a single chroma plane, either U or V doesn't matter
    int single_chroma_plane_len = ((width * height)/4);

    //Points to starting of chroma plane of source buffer
    pixel *src_chroma = src_buf + luma_plane_len;

    //Points to starting of chroma plane of destination buffer
    pixel *dst_chroma = dst_buf + luma_plane_len;

    //Copy Y-Plane as it is from source to destination
    memcpy(dst_buf, src_buf, luma_plane_len);

    //Convert interleaved UV to planner UV and Copy to destination buffer
    int i, j;
    for(i = 0, j = 0; i < (single_chroma_plane_len * 2); i=i+2, j++) {

        //Copy U Component
        dst_chroma[j] = src_chroma[i];
        //Copy V Component
        dst_chroma[j + single_chroma_plane_len] = src_chroma[i+1];
    }
}

/*
 * This function will initialize the incoming
 * timeval structure with current date-time
 * */
void startTimer(struct timeval *t_start)
{
    //Store current time in t_start
    gettimeofday(t_start, NULL);
}

/*
 * This function will read frames in nv12 tile format from input file,
 * converts the frame to nv12 or yuv420p format and write it to output file.
 *
 * params  :- pointer to nv12tile_params structure.
 * infile  :- input file descriptor.
 * outfile :- output file descriptor.
 * */
void read_convert_write(struct nv12tile_params *params, int infile, int outfile)
{
    //Loop to convert nv12 tile formated frames in input file to nv12 format
    //and store it in a output file
    while(1) {

        //Read from source file
        if(params->frame_size_src == read(infile, params->src_buf, params->frame_size_src)) {

            //Convert frames from NV12 tile format to NV12 format
            ConvertNV12TileToNV12Frame(params);

            //If required format of output frame is yuv420p
            //then covert NV12 data to yuv420 planner data
            if(strcmp(OUTPUT_FORMAT, "yuv420p") == 0) {

                //NOTE : using the same source and destination buffers
                //as used in nv12 tile to nv12 conversion.

                //Clear source buffer
                memset(params->src_buf, '\0', sizeof(pixel) * params->frame_size_src);

                //Copy destination buffer to source buffer
                memcpy(params->src_buf, params->dst_buf, params->frame_size_src);

                //Clear destination buffer
                memset(params->dst_buf, '\0', sizeof(pixel) * params->frame_size_src);

                //Convert source buffer in nv12 format
                //to yuv420p format data and Copy to
                //destination buffer
                NV12toYUV420Planner(params->dst_buf, params->src_buf, params->width, params->height);
            }

            //Write to destination file
            if(params->frame_size_dst != write(outfile, params->dst_buf, params->frame_size_dst)) {

                perror("Error writing output file :");
                break;
            }
        }
        else {

            //EOF reached
            break;
        }

        printf("No of frames converted : %d\r", params->frameCount++); fflush(stdout);

        //Clear source and destination buffer
        memset(params->src_buf, '\0', sizeof(pixel) * params->frame_size_src);
        memset(params->dst_buf, '\0', sizeof(pixel) * params->frame_size_src);
    }
}

/*
 * This functions find difference between
 * current date-time and the incoming
 * timeval structure.
 *
 * Returns the difference in milliseconds.
 * */
long stopTimer(struct timeval *t_start)
{
    struct timeval t_end;
    long seconds, useconds;

    //Store current time in t_end
    gettimeofday(&t_end, NULL);

    //Convert seconds and microseconds into milliseconds
    seconds  = t_end.tv_sec  - t_start->tv_sec;
    useconds = t_end.tv_usec - t_start->tv_usec;
    return (((seconds) * 1000 + useconds/1000.0) + 0.5);
}

int main(int argc, char *argv[])
{
    //Check : command line must consist of all required arguments
    if(5 == argc) {

        //Check : input and output file shouldn't be same
        if(strcmp(argv[1], argv[4]) != 0) {

            int infile = -1;

            //Open input/source file for reading
            if(-1 != (infile = open(argv[1], O_RDONLY))) {

                int outfile = -1;

                //Open output/destination file for writing
                if(-1 != (outfile = open(argv[4], O_WRONLY | O_CREAT, 0775))) {

                    //Initialize parameters needed for a nv12 tile buffer
                    struct nv12tile_params *params = NULL;

                    if(NULL != (params = nv12tile_params_init(atoi(argv[2]), atoi(argv[3])))) {

                        //Debug print
                        printf("TILE_SIZE = %d\n"
                                "wTiles=%d(%d)->(%d)\n"
                                "hTiles=%d(%d)->(%d)\n"
                                "hTiles_UV=%d\n"
                                "frame_size_src_Y=%d\n"
                                "frame_size_src_UV=%d\n"
                                "frame_size_src=%d\n"
                                "frame_size_dst=%d\n",
                                TILE_SIZE,
                                params->wTiles,
                                params->width,
                                params->ex_width,
                                params->hTiles,
                                params->height,
                                params->ex_height,
                                params->hTiles_UV,
                                params->frame_size_src_Y,
                                params->frame_size_src_UV,
                                params->frame_size_src,
                                params->frame_size_dst);

                        //Variables required to calculate time elapsed for converting N (frameCount) frames
                        struct timeval start;
                        long timeelapsed = 0;

                        //Start timer
                        startTimer(&start);

                        //Read from infile, convert frames and write to outfile
                        read_convert_write(params, infile, outfile);

                        //Stop timer
                        timeelapsed = stopTimer(&start);

                        //Debug print
                        printf("\rNo of frames converted : %d in %lf seconds\n", params->frameCount, ((double)timeelapsed/1000.0));
                        printf("Display resolution of each frame is %dx%d\n", params->width, params->height);
                    }

                    //deinit params
                    nv12tile_params_deinit(params);

                    //Close output file
                    close(outfile);
                }
                else {

                    perror("Unable to open output file :");
                }

                //Close input file
                close(infile);
            }
            else {

                perror("Unable to open input file :");
            }
        }
        else {

            printf("Input file name and output file name can't be same\n");
        }

    }
    else {

        printf("Usage: %s input_file width height output_file\n", argv[0]);
    }

    return 0;
}
