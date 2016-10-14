#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

//Macro to define output format, either "nv12" or "yuv420p", no other options available
#define OUTPUT_FORMAT "nv12"

//Macro to round up a number "num" to the given number "X"
#define ROUND_UP_X(num,x) (((num)+(x-1))&~(x-1))

//Size of a single tile
#define TILE_SIZE (64*32)

/* Structure to hold required parameters for a nv12 tile buffer.
 * wTiles               - No of tiles required horizontally for both Luma and Chroma Planes.
 * hTiles               - No of tiles required vertically for Luma Plane
 * hTiles_UV            - No of tiles required vertically for Chroma Plane
 * frame_size_src_Y     - Size of Luma Plane
 * frame_size_src_UV    - Size of Chroma Plane
 * */
typedef struct nv12tile_params {
    int wTiles;
    int hTiles;
    int hTiles_UV;
    int frame_size_src_Y;
    int frame_size_src_UV;
} nv12tile_params;

/*
 * This function returns no of tiles required
 * for given width.
 * */
int calc_wTiles(int width)
{
    return (ROUND_UP_X(width,128)/64);
}

/*
 * This function returns no of tiles required
 * for given height.
 * */
int calc_hTiles(int height)
{
    return (ROUND_UP_X(height,32)/32);
}

/*
 * This function will returns padding required
 * for given width and height.
 * */
int calc_boundary_padding(int width,int height)
{
    int wTiles = calc_wTiles(width);
    int hTiles = calc_hTiles(height);
    float data_size = ((float)((float)(((float)wTiles * (float)hTiles * (float)TILE_SIZE)/(float)(4 * TILE_SIZE))));
    data_size = data_size - (float)((int)data_size);
    return (int)(data_size * (float)(4 * TILE_SIZE));
}

/*
 * This function will returns size required
 * for a plane of given width and height.
 * */
int calc_plane_size(int width,int height)
{
    return ((TILE_SIZE * calc_wTiles(width) * calc_hTiles(height)) + calc_boundary_padding(width,height));
}

/*
 * This function will initialize parameter needed
 * for a nv12 tile buffer of given width and height.
 * */
struct nv12tile_params *nv12tile_parmas_init(int width,int height)
{
    struct nv12tile_params *p = calloc(1,sizeof(struct nv12tile_params));

    //Minimum no of columns required
    p->wTiles = calc_wTiles(width);

    //Minimum no of rows required for Y plane
    p->hTiles = calc_hTiles(height);

    //Minimum no of rows required for UV plane
    p->hTiles_UV = calc_hTiles(height/2);

    //Size of Luma/Y Plane
    p->frame_size_src_Y = calc_plane_size(width,height);

    //Size of Chroma/UV Plane
    p->frame_size_src_UV = calc_plane_size(width,height/2);

    return p;
}

/*
 * This functions copy a single tile from source to destination.
 * */
void copyTile(char *dst,char **src,int wTiles)
{
    //Loop variable
    int loop = 0;

    //Run loop for all rows in a tile i.e. from row 0 to row 31
    for(loop=0;loop<32;loop++)
    {
        //Copy a single row from source to destination
        memcpy(dst,*src,64 * sizeof(char));

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
void NV12TileToNV12(char* dst_head,char* src_head,int wTiles,int hTiles)
{
    //Loop variable
    int loop = 0;

    //For Z Flip Z pattern
    int Z = 0;

    //Assign source to source head pointer
    char *src = src_head;

    //Assign destination to destination head pointer
    char *dst = dst_head;

    //For a proper Z Flip Z pattern two consecutive row are needed to be processed
    for(loop = 0; loop < (hTiles%2 == 0 ? hTiles : hTiles-1); loop=loop+2)
    {
        //Put src pointer at start of current row
        src = src_head;

        //Copy Z and flip Z pattern to destination
        for(Z = 0; Z < (wTiles/2); Z++)
        {
            //Put dst pointer at start of current row
            dst = dst_head;

            if(Z%2 == 0)
            {
                //For Z pattern

                //Tile Nos: 0-8-16-...
                dst = dst + (Z * 2 * 64);
                copyTile(dst,&src,wTiles);

                //Tile Nos: 1-9-17-...
                dst = dst + 64;
                copyTile(dst,&src,wTiles);

                //Tile Nos: 2-10-18-...
                dst = dst_head + (wTiles * TILE_SIZE);
                dst = dst + (Z * 2 * 64);
                copyTile(dst,&src,wTiles);

                //Tile Nos: 3-11-19-...
                dst = dst + 64;
                copyTile(dst,&src,wTiles);
            }
            else
            {
                //For Flip Z pattern

                //Tile Nos: 4-12-...
                dst = dst_head + (wTiles * TILE_SIZE);
                dst = dst + (Z * 2 * 64);
                copyTile(dst,&src,wTiles);

                //Tile Nos: 5-13...
                dst = dst + 64;
                copyTile(dst,&src,wTiles);

                //Tile Nos: 6-14-...
                dst = dst_head + (Z * 2 * 64);
                copyTile(dst,&src,wTiles);

                //Tile Nos: 7-15-...
                dst = dst + 64;
                copyTile(dst,&src,wTiles);
            }
        }

        //Move source pointer by two chunk rows
        src_head = src_head + (2 * TILE_SIZE * wTiles);

        //Move destination pointer by two chunk rows
        dst_head  = dst_head + (2 * wTiles * TILE_SIZE);
    }

    //For linear pattern in the last remaining row
    if(hTiles%2)
    {
        //Put src pointer at start of current row
        src = src_head;

        //Loop through linear tiles
        for(loop = 0; loop < wTiles ; loop++)
        {
            dst = dst_head + (loop * 64);
            copyTile(dst,&src,wTiles);
        }
    }
}


/*
 * This function is use to convert extrapolated nv12 data
 * to actual nv12 data.
 *
 * dst_head         :- destination pointer to store data in actual nv12 data
 * src_head         :- source pointer, pointing to a extrapolated nv12 data
 * actual_width     :- Actual width of data in source buffer
 * width            :- Width of source buffer
 * actual_height    :- Actual height of data in source buffer
 * */
void ConvertToActualNV12(char *dst_buf, char *src_buf, int actual_width, int width, int actual_height)
{
    int index = 0;
    int uptoIndex = ((actual_height * 3)/2);

    while(index <= uptoIndex)
    {
        //Copy source to destination
        memcpy(dst_buf + (index * actual_width), src_buf + (index * width), (actual_width * sizeof (char)));

        index++;
    }
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
void NV12toYUV420Planner(char *dst_buf,char *src_buf,int width,int height)
{
    //Length of a luma plane
    int luma_plane_len = (width * height);

    //Length of a single chroma plane, either U or V doesn't matter
    int single_chroma_plane_len = ((width * height)/4);

    //Points to starting of chroma plane of source buffer
    char *src_chroma = src_buf + luma_plane_len;

    //Points to starting of chroma plane of destination buffer
    char *dst_chroma = dst_buf + luma_plane_len;

    //Copy Y-Plane as it is from source to destination
    memcpy(dst_buf,src_buf,luma_plane_len);

    //Convert interleaved UV to planner UV and Copy to destination buffer
    int i,j;
    for(i = 0, j = 0; i < (single_chroma_plane_len * 2); i=i+2, j++)
    {
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
 * This functions find difference between
 * current date-time and the incoming
 * timevale structure.
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

int main(int argc, char* argv[])
{
    //Check : command line must consist of all required arguments
    if(5 == argc)
    {
        //Check : input and output file shouldn't be same
        if(strcmp(argv[1],argv[4]) != 0)
        {
            int infile = -1;
            //Open input/source file for reading
            if(-1 != (infile = open(argv[1],O_RDONLY)))
            {
                int outfile = -1;
                //Open output/destination file for writing
                if(-1 != (outfile = open(argv[4],O_WRONLY | O_CREAT,0775)))
                {
                    //Parse actual width from the command line
                    int actual_width = atoi(argv[2]);

                    //Extrapolate width from the actual width such that it is always in multiple of 128
                    int width = ROUND_UP_X(actual_width,128);

                    //Parse actual height from the command line
                    int actual_height = atoi(argv[3]);

                    //Extrapolate height from the actual height such that it is always in multiple of 32
                    int height = ROUND_UP_X(actual_height,32);

                    //Initialize parameters needed for a nv12 tile buffer
                    struct nv12tile_params *params = nv12tile_parmas_init(width,height);

                    //Size of a single frame in source, source is in nv12 tile format
                    int frame_size_src = params->frame_size_src_Y + params->frame_size_src_UV;

                    //Buffer to accumulate a frame from the source
                    char *src_buf = (char *) calloc(1,frame_size_src * sizeof(char));

                    //Buffer to accumulate a frame for the destination
                    char *dst_buf = (char *) calloc(1,frame_size_src * sizeof(char));

                    //Size of a single frame in destination, destination is in nv12 format
                    int frame_size_dst = (actual_width * actual_height * 3)/2;


                    printf("TILE_SIZE = %d\n"
                            "wTiles=%d(%d)->(%d)\n"
                            "hTiles=%d(%d)->(%d)\n"
                            "hTiles_UV=%d\n"
                            "frame_size_src_Y=%d\n"
                            "frame_size_src_UV=%d\n"
                            "frame_size_src=%d\n"
                            "frame_size_dst=%d\n",
                            TILE_SIZE,
                            params->wTiles,actual_width,width,
                            params->hTiles,actual_height,height,
                            params->hTiles_UV,
                            params->frame_size_src_Y,
                            params->frame_size_src_UV,
                            frame_size_src,
                            frame_size_dst);

                    //Variable to count no of frames converted from tile to linear format i.e. nv12 tile to nv12 format
                    int frameCount = 1;

                    //Variables required to calculate time elapsed for converting N (frameCount) frames
                    struct timeval start;
                    long timeelapsed = 0;

                    //Start timer
                    startTimer(&start);

                    //Loop to convert nv12 tile formated frames in input file to nv12 format
                    //and store it in a output file
                    while(1)
                    {
                        //Read from source file
                        if(params->frame_size_src_Y == read(infile,src_buf,params->frame_size_src_Y))
                        {
                            //Convert Y plane from nv12 tile to nv12 format
                            NV12TileToNV12(dst_buf,src_buf,params->wTiles,params->hTiles);

                            //Clear source buffer
                            memset(src_buf,'\0',sizeof(char) * frame_size_src);

                            //Read from source file
                            if(params->frame_size_src_UV == read(infile,src_buf,params->frame_size_src_UV))
                            {
                                //Convert UV plane from nv12 tile to nv12 format
                                NV12TileToNV12(dst_buf+(width * height),src_buf,params->wTiles,params->hTiles_UV);
                            }
                            else
                            {
                                //break on reach of EOF
                                break;
                            }

                            //If extrapolation is performed, convert extrapolated data
                            //to actual data
                            if((actual_width != width) || (actual_height != height))
                            {
                                //Clear source buffer
                                memset(src_buf,'\0',sizeof(char) * frame_size_src);

                                //Copy destination buffer to source buffer
                                memcpy(src_buf, dst_buf, frame_size_src);

                                //Clear destination buffer
                                memset(dst_buf,'\0',sizeof(char) * frame_size_src);

                                //Convert extrapolated NV12 data to actual NV12 data
                                ConvertToActualNV12(dst_buf, src_buf, actual_width, width, actual_height);
                            }

                            //If required format of output frame is yuv420p
                            //then covert NV12 data to yuv420 planner data
                            if(strcmp(OUTPUT_FORMAT,"yuv420p") == 0)
                            {
                                //NOTE : using the same source and destination buffers
                                //as used in nv12 tile to nv12 conversion.

                                //Clear source buffer
                                memset(src_buf,'\0',sizeof(char) * frame_size_src);

                                //Copy destination buffer to source buffer
                                memcpy(src_buf, dst_buf, frame_size_src);

                                //Clear destination buffer
                                memset(dst_buf,'\0',sizeof(char) * frame_size_src);

                                //Convert source buffer in nv12 format
                                //to yuv420p format data and Copy to
                                //destination buffer
                                NV12toYUV420Planner(dst_buf, src_buf, actual_width, actual_height);
                            }

                            //Write to destination file
                            if(frame_size_dst != write(outfile,dst_buf,frame_size_dst))
                            {
                                perror("Error writing output file :");
                                break;
                            }
                        }
                        else
                        {
                            //break on reach of EOF
                            break;
                        }

                        printf("No of frames converted : %d\r",frameCount++);
                        fflush(stdout);

                        //Clear source and destination buffer
                        memset(src_buf,'\0',sizeof(char) * frame_size_src);
                        memset(dst_buf,'\0',sizeof(char) * frame_size_src);
                    }

                    //Stop timer
                    timeelapsed = stopTimer(&start);

                    //Free params
                    if(params)
                        free(params);

                    //Free source buffer
                    if(src_buf)
                        free(src_buf);

                    //Free destination buffer
                    if(dst_buf)
                        free(dst_buf);

                    //Close output file
                    close(outfile);

                    printf("\rNo of frames converted : %d in %lf seconds\n",frameCount,((double)timeelapsed/1000.0));
                    printf("Display resolution of each frame is %dx%d\n", actual_width, actual_height);
                }
                else
                {
                    perror("Unable to open output file :");
                }

                //Close input file
                close(infile);
            }
            else
            {
                perror("Unable to open input file :");
            }
        }
        else
        {
            printf("Input file name and output file name can't be same\n");
        }

    }
    else
    {
        printf("Usage: %s input_file width height output_file\n",argv[0]);
    }

    return 0;
}
