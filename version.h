#ifndef __VERSION_H__
#define __VERSION_H__

#define VER_MODEL     "MODEL"
#define VER_BOARD     "BOARD"
#define VER_REFERENCE "REFERENCE"
#define VER_NUMBER    "0.9.32.1"

// 각 클래스의 디버그 메시지 on/off 를 위해
// m_debugEnabled 변수를 무조건 만듬. 부모 클래스에서 setProperty 를 통해 접근 가능하게 함.


// 레지스트리 세팅을 공유 해야 하므로 APP_NAME 은 실행 파일 이름과는 다르다.
#define APP_NAME        "ModbusRTU-ASCII Tester"

#define APP_CONFIG_FILE "config.ini"


#define PROGRAM_TITLE                           APP_NAME
#define PROGRAM_WIDTH                           800
#define PROGRAM_HEIGHT                          480  // 14인치 16:10 모니터

#define KOR_TRANSLATION_FILE_PATH               "data/translation/ko_kr.qm"
#define ENG_TRANSLATION_FILE_PATH               "data/translation/en_us.qm"


#endif /* __VERSION_H__ */
