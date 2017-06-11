      real a(10)
      integer i

      do 1 i=1,10
         a(i) = i
 1    continue

      call sm_array_to_vector(a,10,'xyz')
      print *,'Calling SM parser...'
      call sm_parser('-q')
      print *,'Exited parser'
      end
      
         
