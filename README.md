# Avoid F

- - -

## Embedded Programming
## nrf52832(nordic사) + st7586s 를 이용해 만든 게임

## 개요

* 수업을 수행하는 학생이 교수님이 뿌리시는 학점을 피해 A+를 맞으려 노력하는 게임
* F -> D -> C -> B -> A 처럼 학점을 역순으로 나타내 학우와 교수님의 관심을 샀다.
* F , D , C , B 에서 랜덤으로 A가 등장하고 이때의 A는 학생의 시험기회를 1번 늘려준다.
* 시험기회는 우측 상단에 나타냈으며 모두 소모되면 지게 된다.

 - - -
 ## 플레이
 <table>
   <tr>
     <th align="center">
       <img width="200" alt="1" src="https://user-images.githubusercontent.com/11826495/79062706-7a4e8300-7cd7-11ea-822c-2a62250c4156.gif"/>
       <br><br>[Start]
     </th>
     <th align="center">
       <img width="200" alt="1" src="https://user-images.githubusercontent.com/11826495/79062822-5dff1600-7cd8-11ea-983a-181ca71912f1.gif"/>
       <br><br>[Get A] 
    </th>
     <th align="center">
      <img width="200" alt="1" src="https://user-images.githubusercontent.com/11826495/79062868-c64df780-7cd8-11ea-9156-3713c43e945c.gif"/>
       <br><br>[Last Stage]
    </th>
     <th align="center">
      <img width="200" alt="1" src="https://user-images.githubusercontent.com/11826495/79062922-30669c80-7cd9-11ea-93c1-588d61bfff07.gif"/>
       <br><br>[Ending]
    </th>
  </tr>
</table>


- - -

### Issue

* Interrupt를 처리하는 데에 시간을 많이 사용했다.
  * Timer Interrupt와 IO장치의 Interrupt를 동시에 Interrupt 큐에 담았을 때 IO Interrupt는 Event가 발생하지 않았음.
    * 해결을 위해 Interrupt Event Handler를 전면 수정하게 됨.
  
  - - - 
  
* Interrupt의 문제를 해결하고 난 뒤 IO장치의 Interrupt가 들어오면 Timer가 멈추는 현상 발생.
  * 이후 Library를 찾아보니 Schedule을 다룰 수 있는 함수를 발견
    * app_sched_execute() 메소드를 이용해 Interrupt를 처리
    
     - - - 
    
* 현재까지 개발한 프로젝트 중 Line 수로만 따지면 가장 긴 프로젝트
  * 그중 90%가 UI를 구성하는 Code
    * 마친 후에 든 생각이지만 각 Object들을 struct로 구성하여 개발했으면 Code가 훨씬 줄었을 것이다.
    
     - - - 
     
### 전체 플레이 영상

#### 클릭하면 실행됩니다.

[![Video Label](http://img.youtube.com/vi/oYZV6ba-SUc/0.jpg)](https://youtu.be/oYZV6ba-SUc?t=0s)

- - -
     
### 마친 후

* H/W의 구동원리를 이해하게 됨.
* 이론으로만 알던 Event Queue를 다뤄보는 좋은 기회였다.

 - - - 

