	integer i,NXY
	parameter(NXY=20)
	real lev(NXY),z(NXY,NXY)
c
	call fsm_device('hirez')
	call fsm_location(5000,30000,5000,30000)
	do 2 i=1,NXY
		do 1 j=1,NXY
			z(i,j)=(i-10)**2 + (j - 1)**2
1		continue
2	continue
c
	call fsm_graphics
	call fsm_limits(-1.0,21.0,-1.0,21.0)
	call fsm_box(1,2,0,0)
	call fsm_defimage(z,0.,20.,0.,20.,NXY,NXY)
	call fsm_minmax(amin,amax)
	do 3 i=1,NXY
		lev(i)=amin + (i - 1.)/(NXY - 1)*(amax - amin)
3	continue
	call fsm_levels(lev,NXY)
	call fsm_contour
	call fsm_hardcopy
	call fsm_alpha
	end
