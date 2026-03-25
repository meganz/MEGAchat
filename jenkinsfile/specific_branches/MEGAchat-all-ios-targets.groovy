def xcframeworkMessage(artifacts, String log_url, String sdk_commit, String megachat_commit) {
    def urls_with_checksums = ""
    def message = ""
    def statusMessage = currentBuild.currentResult == "SUCCESS" ?
        "🚀 XCFrameworks published to Artifactory."
        : "❌ There was an error building XCFrameworks."
    artifacts.each { artifact ->
        urls_with_checksums += " * [${artifact[0]}](${artifact[1]})\n    * checksum: `${artifact[2]}`\n"
    }
    urls_with_checksums = urls_with_checksums ?: "⚠️ _No links available_"
    message = """
${statusMessage} Job status: `${currentBuild.currentResult}`.

 * SDK commit:	`${sdk_commit}`
 * MEGAChat commit: `${megachat_commit}`

Links:
${urls_with_checksums}

🪵 [Build Logs](${log_url})
"""
    return message
}

def slackMessage(String message) {
    String body = message + "\nCalled from [MR #${gitlabMergeRequestIid}](${GIT_URL_IOS_MRS}/${gitlabMergeRequestIid})"
    String messageColor = currentBuild.currentResult == 'SUCCESS'? "#00FF00": "#FF0000"

    // We need to convert the links from Markdown to Slack-style
    String tmp = ""
    def linkRegex = ~"\\[(.+?)\\]\\((.+?)\\)"
    while (tmp != body) {
        tmp = body
        body = body.replaceFirst(linkRegex, "<\$2 | \$1>")
    }

    withCredentials([string(credentialsId: 'SLACK_WEBHOOK_IOS_SDK_PIPELINE', variable: 'SLACK_WEBHOOK_URL')]) {
        sh """
            curl -sS -X POST \
                -H 'Content-type: application/json' \
                --data '{"attachments": [
                            {
                                "color": "${messageColor}",
                                "blocks": [{
                                    "type": "section",
                                    "text": {
                                            "type": "mrkdwn",
                                            "text": "${body}"
                                    }
                                }]
                            }
                        ]}' \
                \${SLACK_WEBHOOK_URL} \
            || true
        """
    }
}

def xcframeworkChecksumUpload(String file, String version, Boolean swiftChecksum) {
    def sha256 = swiftChecksum ?
          sh(script: "swift package compute-checksum ${file}", returnStdout: true).trim()
        : sh(script: "sha256 -q ${file}", returnStdout: true).trim()
    def sha1 =  sh(script: "sha1 -q ${file}", returnStdout: true).trim()
    def md5 =  sh(script: "md5 -q ${file}", returnStdout: true).trim()
    def targetPath = "${REPOSITORY_URL}/artifactory/ios-mega/xcframework/$version/"
    withCredentials([string(credentialsId: 'IOS-MEGA-ARTIFACTORY-UPLOAD', variable: 'ARTIFACTORY_TOKEN')]) {
        sh """
            curl -X PUT\
                -H "Authorization: Bearer \$ARTIFACTORY_TOKEN" \
                -H "X-Checksum-Sha256:${sha256}" \
                -H "X-Checksum-Sha1:${sha1}" \
                -H "X-Checksum-md5:${md5}" \
                -T "${file}" \
                "${targetPath}"
        """
    }
    return [file, targetPath + file, sha256]
}

void downloadJenkinsConsoleLog(String fileName) {
    withCredentials([usernameColonPassword(credentialsId: 'jenkins-ro', variable: 'CREDENTIALS')]) {
        sh "curl -u \"\${CREDENTIALS}\" ${BUILD_URL}consoleText -o ${fileName}"
    }
}

String getLogsUrl(String fileName, String version) {
    String logsUrl = ""
    downloadJenkinsConsoleLog(fileName)
    (_, logsUrl, _) = xcframeworkChecksumUpload(fileName, version, false)
    return logsUrl
}

def build_xcframework = false
def artifacts = []

pipeline {
    agent { label 'osx && arm64' }
    options {
        timeout(time: 300, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
    }
    parameters {
        booleanParam(name: 'RESULT_TO_SLACK', defaultValue: true, description: 'Should the job result be sent to slack?')
        string(name: 'MEGACHAT_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
        string(name: 'SDK_BRANCH', defaultValue: 'develop', description: 'Define a custom SDK branch.')
    }
    stages {
        stage('Override parameters') { // Overrides parameters when this jobs is called via GitLab webhook
            when {
                expression { return env.gitlabTriggerPhrase?.trim() }
            }
            steps {
                script {
                    def WEBHOOK_SDK_BRANCH = sh(script: 'echo "$gitlabTriggerPhrase" | grep --only-matching "\\-\\-sdk-branch=[^ ]*" | awk -F "sdk-branch="  \'{print \$2}\'|| :', returnStdout: true).trim()
                    def WEBHOOK_MEGACHAT_BRANCH = sh(script: 'echo "$gitlabTriggerPhrase" | grep --only-matching "\\-\\-chat-branch=[^ ]*" | awk -F "chat-branch="  \'{print \$2}\'|| :', returnStdout: true).trim()

                    env.SDK_BRANCH       = WEBHOOK_SDK_BRANCH ?: params.SDK_BRANCH
                    env.MEGACHAT_BRANCH  = WEBHOOK_MEGACHAT_BRANCH ?: params.MEGACHAT_BRANCH
                    env.RESULT_TO_SLACK  = false
                    build_xcframework = true

                    addGitLabMRComment comment: "🚧⌛ Building XCFramework.\n\n🔗 [Job #${BUILD_NUMBER}](${BUILD_URL})."
                }
            }
        }
        stage('Checkout SDK and MEGAchat'){
            steps {
                deleteDir()
                sh "echo Cloning MEGAchat branch \"${env.MEGACHAT_BRANCH}\""
                checkout([
                    $class: 'GitSCM',
                    branches: [[name: "${env.MEGACHAT_BRANCH}"]],
                    userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:megachat/MEGAchat.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                    extensions: [
                        [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                    ]
                ])
                dir('third-party/mega'){
                    sh "echo Cloning SDK branch \"${env.SDK_BRANCH}\""
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "${env.SDK_BRANCH}"]],
                        userRemoteConfigs: [[ url: "git@code.developers.mega.co.nz:sdk/sdk.git", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"]
                        ]
                    ])
                }
                script{
                    megachat_sources_workspace = WORKSPACE
                    sdk_sources_workspace = "${megachat_sources_workspace}/third-party/mega"
                }
            }
        }


        stage('Build iOS'){
            options{
                timeout(time: 120, unit: 'MINUTES')
            }
            environment{
                PATH = "/usr/local/bin:${env.PATH}"
                VCPKGPATH = "${env.HOME}/jenkins/vcpkg"
                BUILD_DIR_ARM64 = "build_dir_arm64"
                BUILD_DIR_ARM64_SIM = "build_dir_arm64_simulator"
                VCPKG_BINARY_SOURCES = 'clear;x-aws,s3://vcpkg-cache/archives/,readwrite'
                AWS_ACCESS_KEY_ID = credentials('s4_access_key_id_vcpkg_cache')
                AWS_SECRET_ACCESS_KEY = credentials('s4_secret_access_key_vcpkg_cache')
                AWS_ENDPOINT_URL = "https://s3.g.s4.mega.io"
            }
            steps{
                //Build MEGAchat for arm64
                sh "echo Building MEGAchat for iOS arm64"
                sh "cmake --preset mega-ios -DVCPKG_TARGET_TRIPLET=arm64-ios-mega -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVCPKG_ROOT=${VCPKGPATH} -DCMAKE_VERBOSE_MAKEFILE=ON -S ${WORKSPACE} -B ${WORKSPACE}/${BUILD_DIR_ARM64}"
                sh "cmake --build ${WORKSPACE}/${BUILD_DIR_ARM64} -j2"

                //Build MEGAchat for arm64 simulator
                sh "echo \"Building MEGAchat for iOS arm64 simulator (crosscompiling)\""
                sh "cmake --preset mega-ios -DVCPKG_TARGET_TRIPLET=arm64-ios-simulator-mega -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVCPKG_ROOT=${VCPKGPATH} -DCMAKE_VERBOSE_MAKEFILE=ON -S ${WORKSPACE} -B ${WORKSPACE}/${BUILD_DIR_ARM64_SIM}"
                sh "cmake --build ${WORKSPACE}/${BUILD_DIR_ARM64_SIM} -j2"
            }
        }
        stage ("Build and upload iOS xcframework") {
            when { expression { return  build_xcframework } }
            environment {
                BUILD_DIR_ARM64 = "build_dir_arm64"
                BUILD_DIR_ARM64_SIM = "build_dir_arm64_simulator"
                IOS_DIR="ios"

                // These are the directories where build-sdk-libs.sh expects the libs being built
                BUILD_DIR_DEVICE="BUILD_ARM64_iOS"
                BUILD_DIR_SIMULATOR="BUILD_ARM64_simulator"
            }
            steps {
                script {
                    env.VERSION = sh(script: "date '+%Y%m%d.%H%M%S'", returnStdout: true).trim()
                }
                dir("${IOS_DIR}"){
                    checkout([  // Checkouts the source and target branches merged, from the webhook MR
                                // Just in case devs have modified `build-sdk-libs.sh` script in the MR
                        $class: 'GitSCM',
                        branches: [[name: "origin/${env.gitlabSourceBranch}"]],
                        userRemoteConfigs: [[ url: "${GIT_URL_IOS}", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                        extensions: [
                            [$class: "UserIdentity",name: "jenkins", email: "jenkins@jenkins"],
                            [$class: 'PreBuildMerge', options: [fastForwardMode: 'FF', mergeRemote: "origin", mergeStrategy: 'DEFAULT', mergeTarget: "${env.gitlabTargetBranch}"]]
                        ]
                    ])
                    sh """
                        # Link built libraries
                        rm -rf ${BUILD_DIR_DEVICE}
                        rm -rf ${BUILD_DIR_SIMULATOR}
                        ln -s ${WORKSPACE}/${BUILD_DIR_ARM64} ${BUILD_DIR_DEVICE}
                        ln -s ${WORKSPACE}/${BUILD_DIR_ARM64_SIM} ${BUILD_DIR_SIMULATOR}

                        # Link SDK and MEGAchat
                        rm -rf Modules/DataSource/MEGAChatSDK/Sources/MEGAChatSDK
                        rm -rf Modules/DataSource/MEGASDK/Sources/MEGASDK
                        ln -s ${megachat_sources_workspace} Modules/DataSource/MEGAChatSDK/Sources/MEGAChatSDK
                        ln -s ${sdk_sources_workspace} Modules/DataSource/MEGASDK/Sources/MEGASDK

                        # Export the right path
                        export PATH="\$(xcode-select -p)/Toolchains/XcodeDefault.xctoolchain/usr/bin:\$PATH"
                        bash -x scripts/build-sdk-libs.sh --skip-build-libs

                        # Compress xcframework
                        for i in `ls xcframework`; do
                            zip -r xcframework/\$i.${env.VERSION}.zip xcframework/\$i
                        done
                    """

                    // Upload xcframework
                    dir("xcframework") {
                        script {
                            def files = sh(script:"ls *.zip", returnStdout: true).split("\n")
                            files.each { file ->
                                artifacts << xcframeworkChecksumUpload(file, env.VERSION, true)
                            }
                        }
                    }
                }
            }
            post {
                always {
                    // Post message about the new artifacts
                    script {
                        def sdk_commit = sh(script: "git -C ${sdk_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                        def megachat_commit = sh(script: "git -C ${megachat_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                        def logUrl = getLogsUrl("build.log", env.VERSION)
                        def message = xcframeworkMessage(artifacts, logUrl, sdk_commit, megachat_commit)
                        echo message
                        addGitLabMRComment comment: message
                        slackMessage(message)
                    }
                }
            }
        }
    }
    post {
        always {
            script {
                if (env.RESULT_TO_SLACK.toBoolean()) {
                    def sdk_commit = sh(script: "git -C ${sdk_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def megachat_commit = sh(script: "git -C ${megachat_sources_workspace} rev-parse HEAD", returnStdout: true).trim()
                    def messageStatus = currentBuild.currentResult
                    def messageColor = messageStatus == 'SUCCESS'? "#00FF00": "#FF0000" //green or red
                    def message = """
                        *MEGAchat iOS* <${BUILD_URL}|Build result>: '${messageStatus}'.
                        MEGAchat branch: `${MEGACHAT_BRANCH}`
                        MEGAchat commit: `${megachat_commit}`
                        SDK branch: `${env.SDK_BRANCH}`
                        SDK commit: `${sdk_commit}`
                    """.stripIndent()

                    withCredentials([string(credentialsId: 'slack_webhook_sdk_report', variable: 'SLACK_WEBHOOK_URL')]) {
                        sh """
                            curl -X POST -H 'Content-type: application/json' --data '
                                {
                                "attachments": [
                                    {
                                        "color": "${messageColor}",
                                        "blocks": [
                                        {
                                            "type": "section",
                                            "text": {
                                                    "type": "mrkdwn",
                                                    "text": "${message}"
                                            }
                                        }
                                        ]
                                    }
                                    ]
                                }' \${SLACK_WEBHOOK_URL}
                         """
                    }
                }
            }
            deleteDir() /* clean up our workspace */
        }
    }
}
