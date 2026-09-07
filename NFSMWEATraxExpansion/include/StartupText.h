#pragma once
#include <Windows.h>
#include <filesystem>
#include <string>
#include <cwctype>
namespace eatrax::startup {
struct Text { const wchar_t *language,*title,*intro,*cancel,*cancelling,*checking,*generating,*verifying,*complete,*restartTitle,*restartBody,*elapsed; };
inline const Text translations[]={
    {L"English", L"Preparing music", L"Preparing added music for the game. The first run may take a few minutes. Later starts reuse the cache.", L"Cancel and use standard music", L"Cancelling. This launch will use standard music.", L"Checking music and cache.", L"Generating music cache", L"Verifying prepared music.", L"Generation complete.", L"Restart required", L"Music cache generation is complete. Restart the game to apply the changes.\r\n\r\nPress OK to close the game, then launch it again. The next launch will use the completed cache.", L"Elapsed"},
    {L"Japanese", L"音楽の起動準備", L"追加した音源をゲーム用に準備しています。初回は数分かかります。次回からはキャッシュを再利用します。", L"中止して標準BGMで起動", L"変換を中止しています。今回は標準BGMで起動します。", L"音源とキャッシュを確認しています。", L"音楽キャッシュを生成中", L"変換済み音源を検証しています。", L"生成が完了しました。", L"再起動が必要です", L"音楽キャッシュの生成が完了しました。変更を反映するため、ゲームを再起動してください。\r\n\r\nOKを押すとゲームを終了します。もう一度ゲームを起動してください。次回は生成済みキャッシュを使用します。", L"経過時間"},
    {L"French", L"Préparation de la musique", L"Préparation des morceaux ajoutés. La première génération peut prendre quelques minutes. Le cache sera réutilisé ensuite.", L"Annuler et utiliser la musique standard", L"Annulation. La musique standard sera utilisée.", L"Vérification de la musique et du cache.", L"Génération du cache musical", L"Vérification de la musique préparée.", L"Génération terminée.", L"Redémarrage nécessaire", L"Le cache musical est prêt. Redémarrez le jeu pour appliquer les changements.\r\n\r\nCliquez sur OK pour fermer le jeu, puis relancez-le. Le cache sera réutilisé.", L"Temps écoulé"},
    {L"German", L"Musik wird vorbereitet", L"Die hinzugefügte Musik wird vorbereitet. Beim ersten Start kann dies einige Minuten dauern. Danach wird der Cache wiederverwendet.", L"Abbrechen und Standardmusik nutzen", L"Abbruch. Die Standardmusik wird verwendet.", L"Musik und Cache werden geprüft.", L"Musik-Cache wird erstellt", L"Vorbereitete Musik wird geprüft.", L"Erstellung abgeschlossen.", L"Neustart erforderlich", L"Der Musik-Cache ist fertig. Starten Sie das Spiel neu, um die Änderungen anzuwenden.\r\n\r\nKlicken Sie auf OK, um das Spiel zu schließen, und starten Sie es erneut.", L"Verstrichen"},
    {L"Italian", L"Preparazione della musica", L"Preparazione dei brani aggiunti. La prima volta può richiedere alcuni minuti. In seguito verrà riutilizzata la cache.", L"Annulla e usa la musica standard", L"Annullamento. Verrà usata la musica standard.", L"Controllo della musica e della cache.", L"Generazione della cache musicale", L"Verifica della musica preparata.", L"Generazione completata.", L"Riavvio necessario", L"La cache musicale è pronta. Riavvia il gioco per applicare le modifiche.\r\n\r\nPremi OK per chiudere il gioco, poi avvialo nuovamente. La cache verrà riutilizzata.", L"Tempo trascorso"},
    {L"Spanish", L"Preparando la música", L"Preparando las canciones añadidas. La primera vez puede tardar unos minutos. Después se reutilizará la caché.", L"Cancelar y usar música estándar", L"Cancelando. Se usará la música estándar.", L"Comprobando música y caché.", L"Generando la caché de música", L"Verificando la música preparada.", L"Generación completada.", L"Es necesario reiniciar", L"La caché de música está lista. Reinicia el juego para aplicar los cambios.\r\n\r\nPulsa OK para cerrar el juego y vuelve a iniciarlo. Se reutilizará la caché.", L"Tiempo transcurrido"},
    {L"Dutch", L"Muziek voorbereiden", L"De toegevoegde muziek wordt voorbereid. Dit kan de eerste keer enkele minuten duren. Daarna wordt de cache hergebruikt.", L"Annuleren en standaardmuziek gebruiken", L"Annuleren. De standaardmuziek wordt gebruikt.", L"Muziek en cache controleren.", L"Muziekcache genereren", L"Voorbereide muziek controleren.", L"Genereren voltooid.", L"Opnieuw starten vereist", L"De muziekcache is klaar. Start het spel opnieuw om de wijzigingen toe te passen.\r\n\r\nKlik op OK om het spel af te sluiten en start het daarna opnieuw.", L"Verstreken"},
    {L"Swedish", L"Förbereder musik", L"Förbereder tillagd musik. Första gången kan det ta några minuter. Därefter återanvänds cachen.", L"Avbryt och använd standardmusik", L"Avbryter. Standardmusiken används.", L"Kontrollerar musik och cache.", L"Skapar musikcache", L"Verifierar förberedd musik.", L"Klart.", L"Omstart krävs", L"Musikcachen är klar. Starta om spelet för att tillämpa ändringarna.\r\n\r\nTryck på OK för att stänga spelet och starta det sedan igen.", L"Förfluten tid"},
    {L"Danish", L"Forbereder musik", L"Forbereder tilføjet musik. Første gang kan det tage nogle minutter. Derefter genbruges cachen.", L"Annuller og brug standardmusik", L"Annullerer. Standardmusikken bruges.", L"Kontrollerer musik og cache.", L"Opretter musikcache", L"Kontrollerer den forberedte musik.", L"Færdig.", L"Genstart påkrævet", L"Musikcachen er klar. Genstart spillet for at anvende ændringerne.\r\n\r\nTryk på OK for at lukke spillet, og start det derefter igen.", L"Forløbet tid"},
    {L"Polish", L"Przygotowywanie muzyki", L"Przygotowywanie dodanej muzyki. Za pierwszym razem może to potrwać kilka minut. Później pamięć podręczna będzie używana ponownie.", L"Anuluj i użyj standardowej muzyki", L"Anulowanie. Zostanie użyta standardowa muzyka.", L"Sprawdzanie muzyki i pamięci podręcznej.", L"Tworzenie pamięci podręcznej muzyki", L"Sprawdzanie przygotowanej muzyki.", L"Generowanie zakończone.", L"Wymagane ponowne uruchomienie", L"Pamięć podręczna muzyki jest gotowa. Uruchom grę ponownie, aby zastosować zmiany.\r\n\r\nNaciśnij OK, aby zamknąć grę, a następnie uruchom ją ponownie.", L"Czas"},
    {L"Finnish", L"Valmistellaan musiikkia", L"Valmistellaan lisättyä musiikkia. Ensimmäinen kerta voi kestää muutaman minuutin. Tämän jälkeen käytetään välimuistia.", L"Peruuta ja käytä vakiomusiikkia", L"Peruutetaan. Käytetään vakiomusiikkia.", L"Tarkistetaan musiikkia ja välimuistia.", L"Luodaan musiikin välimuistia", L"Tarkistetaan valmisteltua musiikkia.", L"Valmis.", L"Uudelleenkäynnistys vaaditaan", L"Musiikin välimuisti on valmis. Käynnistä peli uudelleen, jotta muutokset tulevat voimaan.\r\n\r\nSulje peli painamalla OK ja käynnistä se sitten uudelleen.", L"Kulunut aika"},
    {L"Korean", L"음악 준비 중", L"추가한 음악을 준비하고 있습니다. 처음에는 몇 분 정도 걸릴 수 있습니다. 다음 실행부터는 캐시를 재사용합니다.", L"취소하고 기본 음악 사용", L"취소 중입니다. 이번에는 기본 음악을 사용합니다.", L"음악과 캐시를 확인하고 있습니다.", L"음악 캐시 생성 중", L"준비된 음악을 검증하고 있습니다.", L"생성이 완료되었습니다.", L"재시작이 필요합니다", L"음악 캐시 생성이 완료되었습니다. 변경 사항을 적용하려면 게임을 다시 시작하세요.\r\n\r\nOK를 누르면 게임이 종료됩니다. 게임을 다시 실행하면 생성된 캐시를 사용합니다.", L"경과 시간"},
    {L"Chinese (Traditional)", L"正在準備音樂", L"正在準備新增的音樂。首次生成可能需要幾分鐘。之後啟動將重用快取。", L"取消並使用原版音樂", L"正在取消。本次將使用原版音樂。", L"正在檢查音樂與快取。", L"正在生成音樂快取", L"正在驗證已準備的音樂。", L"生成完成。", L"需要重新啟動", L"音樂快取已生成。請重新啟動遊戲以套用變更。\r\n\r\n按下 OK 關閉遊戲，然後再次啟動。下次將使用已生成的快取。", L"已用時間"},
    {L"Chinese (Simplified)", L"正在准备音乐", L"正在准备新增的音乐。首次生成可能需要几分钟。之后启动将重用缓存。", L"取消并使用原版音乐", L"正在取消。本次将使用原版音乐。", L"正在检查音乐与缓存。", L"正在生成音乐缓存", L"正在验证已准备的音乐。", L"生成完成。", L"需要重新启动", L"音乐缓存已生成。请重新启动游戏以应用更改。\r\n\r\n按下 OK 关闭游戏，然后再次启动。下次将使用已生成的缓存。", L"已用时间"},
    {L"Thai", L"กำลังเตรียมเพลง", L"กำลังเตรียมเพลงที่เพิ่มเข้ามา ครั้งแรกอาจใช้เวลาหลายนาที ครั้งถัดไปจะใช้แคชเดิม", L"ยกเลิกและใช้เพลงมาตรฐาน", L"กำลังยกเลิก ครั้งนี้จะใช้เพลงมาตรฐาน", L"กำลังตรวจสอบเพลงและแคช", L"กำลังสร้างแคชเพลง", L"กำลังตรวจสอบเพลงที่เตรียมไว้", L"สร้างเสร็จแล้ว", L"ต้องเริ่มเกมใหม่", L"สร้างแคชเพลงเสร็จแล้ว โปรดเริ่มเกมใหม่เพื่อใช้การเปลี่ยนแปลง\r\n\r\nกด OK เพื่อปิดเกม แล้วเปิดเกมอีกครั้ง ครั้งถัดไปจะใช้แคชที่สร้างไว้", L"เวลาที่ผ่านไป"},
    {L"Russian", L"Подготовка музыки", L"Подготовка добавленной музыки. В первый раз это может занять несколько минут. Затем будет использоваться кэш.", L"Отмена: использовать стандартную музыку", L"Отмена. Будет использована стандартная музыка.", L"Проверка музыки и кэша.", L"Создание музыкального кэша", L"Проверка подготовленной музыки.", L"Создание завершено.", L"Требуется перезапуск", L"Музыкальный кэш готов. Перезапустите игру, чтобы применить изменения.\r\n\r\nНажмите OK, чтобы закрыть игру, затем запустите её снова. Будет использован готовый кэш.", L"Прошло"},
};
inline const Text* currentText=&translations[0];
inline std::wstring Normalize(std::wstring text) {
    auto comment=text.find(L';'); if(comment!=std::wstring::npos) text.resize(comment);
    const auto begin=text.find_first_not_of(L" \t\r\n");
    if(begin==std::wstring::npos) return {};
    text=text.substr(begin,text.find_last_not_of(L" \t\r\n")-begin+1);
    for(auto& c:text)c=static_cast<wchar_t>(towlower(c));
    return text;
}
inline const Text& Lookup(const std::wstring& language) {
    const auto key=Normalize(language);
    for(const auto& text:translations) if(Normalize(text.language)==key) return text;
    return translations[0]; // English US/UK and unknown values.
}
inline void SelectLanguage(HMODULE module) {
    wchar_t path[32768]{}; GetModuleFileNameW(module,path,32768);
    const auto ini=std::filesystem::path(path).parent_path()/L"NFSMostWanted.WidescreenFix.ini";
    wchar_t language[256]{};
    GetPrivateProfileStringW(L"LANGUAGE",L"Language",L"",language,256,ini.c_str());
    if(Normalize(language).empty()) {
        HKEY key=nullptr;
        if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\EA Games\\Need for Speed Most Wanted",0,KEY_QUERY_VALUE|KEY_WOW64_32KEY,&key)==ERROR_SUCCESS) {
            DWORD size=sizeof(language),type=0;
            if(RegQueryValueExW(key,L"Language",nullptr,&type,reinterpret_cast<BYTE*>(language),&size)!=ERROR_SUCCESS || type!=REG_SZ) language[0]=0;
            language[255]=0; RegCloseKey(key);
        }
    }
    currentText=&Lookup(language);
}
}
