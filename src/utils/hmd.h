#ifndef _HMD_H
#define _HMD_H

#define NUM_EYES				2
#define NUM_COLOR_CHANNELS		3


typedef struct
{
	float x;
	float y;
} mesh_coord_t;


typedef struct
{
	int		displayPixelsWide;
	int		displayPixelsHigh;
	int		tilePixelsWide;
	int		tilePixelsHigh;
	int		eyeTilesWide;
	int		eyeTilesHigh;
	int		visiblePixelsWide;
	int		visiblePixelsHigh;
	float	visibleMetersWide;
	float	visibleMetersHigh;
	float	lensSeparationInMeters;
	float	metersPerTanAngleAtCenter;
	int		numKnots;
	float	K[11];
	float	chromaticAberration[4];
} hmd_info_t;

typedef struct
{
	float	interpupillaryDistance;
} body_info_t;

int foo();

float MaxFloat( const float x, const float y );
float MinFloat( const float x, const float y );

float EvaluateCatmullRomSpline( float value, float* K, int numKnots );
void GetDefaultHmdInfo( const int displayPixelsWide, const int displayPixelsHigh, hmd_info_t* hmd_info);
void GetDefaultBodyInfo(body_info_t* body_info);

#endif