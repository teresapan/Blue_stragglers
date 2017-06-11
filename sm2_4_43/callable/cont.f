	integer i,NXY
	parameter(NXY=20)
	real lev(NXY),z(NXY,2*NXY)
c
	call sm_device('graphon')
	call sm_location(5000,30000,5000,30000)
	do 2 i=1,NXY
		do 1 j=1,2*NXY
			z(i,j)=(i-10)**2 + ((j - 1)/2.0)**2
1		continue
2	continue
c
	call sm_graphics
	call sm_limits(-1.0,21.0,-1.0,21.0)
	call sm_box(1,2,0,0)
	call sm_defimage(z,0.,20.,0.,20.,NXY,2*NXY)
	call sm_minmax(amin,amax)
	do 3 i=1,NXY
		lev(i)=amin + (i - 1.)/(NXY - 1)*(amax - amin)
3	continue
	call sm_levels(lev,NXY)
	call sm_contour
	call sm_hardcopy
	call sm_alpha
	end
