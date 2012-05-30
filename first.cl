kernel
void
ArrayMult( global const float *dA, const global float *dB, global float *dC )
{
	int gid = get_global_id( 0 );

	dC[gid] = dA[gid] * dB[gid];
}