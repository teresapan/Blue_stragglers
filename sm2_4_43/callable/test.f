	integer i, NXY, key
        integer sm_device
	parameter(NXY=20)
	real*8 x(NXY),y(NXY),err(NXY)
        real*8 pt(NXY)
	real*8 px, py
c
	if(sm_device('X11') .lt. 0) then
           print *,'can''t open device'
           stop
        endif
	do 2 i=1,NXY
		x(i)=i
		y(i)=i*i
		err(i)=10
                pt(i) = 43
2	continue
c
	call sm_graphics
        call sm_defvar('TeX_strings','1')
        call sm_defvar('default_font','bf')
	call sm_expand(1.0001E0)
	call sm_window(2,2,1,1,2,1)
	call sm_limits(0.0E0,3.0E0,0.0E0,500.0E0)
	call sm_ticksize(-1.0E0,0.0E0,0.0E0,0.0E0)
	call sm_box(1,2,3,3)
        call sm_gflush
	call sm_alpha
        print *,'hit any key to continue'
	call sm_graphics
        call sm_curs(px,py,key)
	call sm_alpha
	print *,'You hit ',key,'at (',px,',',py,')'
        call sm_graphics
	call sm_window(1,1,1,1,1,1)
	call sm_location(5000,30000,5000,30000)
	call sm_erase
	call sm_limits(-1.0E0,22.0E0,0.0E0,500.0E0)
	call sm_ticksize(0.0E0,0.0E0,0.0E0,0.0E0)
	call sm_box(1,2,0,0)
	call sm_angle(45.0E0)
	call sm_ptype(pt,NXY)
	call sm_points(x,y,NXY)
	call sm_angle(0.0E0)
	call sm_lweight(1.0E0)
	call sm_expand(2.0E0)
	call sm_identification('Robert {\\oe the Good}')
	call sm_expand(1.0E0)
	call sm_xlabel('x axis')
	call sm_ylabel('Y')
	call sm_errorbar(x,y,err,2,NXY)
	call sm_errorbar(x,y,err,4,NXY)
	call sm_lweight(3.0E0)
	call sm_histogram(x,y,NXY)
	call sm_lweight(1.0E0)
	call sm_ltype(2)
	call sm_conn(x,y,NXY)
	call sm_ltype(0)
	call sm_relocate(10.0E0,100.0E0)
	call sm_label('Hello ')
	call sm_dot
	call sm_draw(20.0E0,100.0E0)
	call sm_putlabel(4,'Goodbye \\point60')
	call sm_grid(0,0)
        call sm_ltype(1)
	call sm_grid(1,1)
        call sm_ltype(0)
        call sm_gflush
	call sm_alpha
        print *,'enter a digit'
        call sm_redraw(0)
        read(5,*) i
        print *,'enter a digit to exit'
        call sm_redraw(0)
        read(5,*) i
        call sm_hardcopy
	end
