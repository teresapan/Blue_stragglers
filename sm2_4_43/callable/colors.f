	integer i, j, NCOL, NXY
        integer sm_device
	parameter(NXY=100, NCOL=200)
        real color(NCOL)
	real x0(NXY),y0(NXY)
	real x(NXY),y(NXY)
        real r
c
	if(sm_device('X11 -cmap -bg black -fg blue') .lt. 0) then
           print *,'can''t open device'
           stop
        endif
c
c set circles
c
	do 1 i=1,NXY
           x0(i)=cos(i*2*3.141592654/(NXY - 1))
           y0(i)=sin(i*2*3.141592654/(NXY - 1))
 1      continue
c
c set colours to a yellow ramp, but keep the first element blue
c
        color(1) = 256*256*255
	do 2 i=2,NCOL
           color(i) = int(255*(i-1)/real(NCOL - 2)) + 
     $          256*(int(255*(i-1)/real(NCOL - 2)) +
     $          256*0)
 2      continue
        call sm_set_ctypes(color,NCOL)
c
c draw a box in blue
c
	call sm_graphics
	call sm_limits(-1.0d0, 1.0d0, -1.0d0, 1.0d0)

        call sm_ctype_i(0)
        call sm_box(1, 2, 0, 0)
c
c and draw a set of circles
c
	do 3 i=1,NCOL - 1
           r = i/real(NCOL)
           do 4 j=1,NXY
              x(j) = r*x0(j)
              y(j) = r*y0(j)
 4         continue
           call sm_ctype_i(i)
           call sm_connect(x, y, NXY)
 3      continue
c
        call sm_gflush
	call sm_alpha
        print *,'enter a digit to exit'
        call sm_redraw(0)
        read(5,*) i
        call sm_hardcopy
	end
