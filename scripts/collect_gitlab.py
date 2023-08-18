import gitlab

#U_BOOT_ID = 824
U_BOOT_ID = 531
U_BOOT_DM_ID = 552

def doit():
    gl = gitlab.Gitlab('https://source.denx.de/',
                       private_token='glpat-NEECH7ssihjVt67C4CvA')
    projects = gl.projects.list(iterator=True)
    #for project in projects:
       #print(project.name, project.id)

    project = gl.projects.get(U_BOOT_ID)
    #for group in project.groups.list():
       #print(group)
       #projects = group.projects
       #projects = group.projects.list()
       #print(projects)

    pipelines = project.pipelines.list(iterator=True)
    for pipeline in pipelines:
        print(pipeline.status)

    return
    #pipelines = project.pipelines.list()
    #print(pipelines)
    pipeline = project.pipelines.get(17394)
    #pipeline = project.pipelines.get(17393)
    #variables = pipeline.variables.list()
    #print(variables)
    print(pipeline.status)
    jobs = pipeline.jobs.list(iterator=True)
    for job in jobs:
        print(job.name)
    #print(jobs)

doit()
